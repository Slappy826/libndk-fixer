#include "main.h"

std::once_flag copy_functions_jvm{},
    copy_functions_jenv{};
std::vector<JNIEnv*> known_envs{};
std::vector<JavaVM*> known_vms{};
unsigned long long jvm_vtable_copy[sizeof(JNIInvokeInterface) / 8]{},
    jenv_vtable_copy[sizeof(JNINativeInterface) / 8]{};
std::unordered_map<std::uintptr_t, std::uintptr_t> redirection_cache{};
std::atomic<bool> allow_call{};
bridge_class* orig_native_bridge = nullptr;
struct sigaction previous_segv;

// While probably not needed, ensures the symbol doesn't get mangled.
extern "C"
{
    bridge_class NativeBridgeItf
    {
        .version = 6,
    };
}

void segv_handler(int signum, siginfo_t *p_act, void* context)
{
    sigcontext* ctx = reinterpret_cast<sigcontext*>(&(((ucontext_t*)context)->uc_mcontext));
    uintptr_t fault_address = ctx->rip;

    // This works by checking if the value held in RDI is a JavaVM or JNIEnv pointer
    // Which lets us know if the violation happened while attempting to call a jni function
    // And then can redirect it to the real function.
    // Redirection is needed as roblox overwrites the function pointers in the functions table
    // and ndk_translation does not copy the values when converting the pointer to a guest value
    // so it attempts to call into roblox's arm code, causing a crash.

    if (redirection_cache.count(fault_address))
    {
        ctx->rip = redirection_cache[fault_address];
        return;
    }
    else if (std::find(known_envs.begin(), known_envs.end(), reinterpret_cast<JNIEnv*>(ctx->rdi)) != known_envs.end())
    {
        auto functions = reinterpret_cast<const unsigned long long*>(reinterpret_cast<JNIEnv*>(ctx->rdi)->functions);

        auto offset = 0;
        for (;; offset++)
            if (functions[offset] == ctx->rip)
                break;
    
        ctx->rip = jenv_vtable_copy[offset];
        redirection_cache[fault_address] = jenv_vtable_copy[offset];
        return;
    }
    else if (std::find(known_vms.begin(), known_vms.end(), reinterpret_cast<JavaVM*>(ctx->rdi)) != known_vms.end())
    {
        auto functions = reinterpret_cast<const unsigned long long*>(reinterpret_cast<JavaVM*>(ctx->rdi)->functions);

        auto offset = 0;
        for (;; offset++)
            if (functions[offset] == ctx->rip)
                break;
    
        ctx->rip = jvm_vtable_copy[offset];
        redirection_cache[fault_address] = jvm_vtable_copy[offset];
        return;
    }
    else
    {
        dprint("failed to match env");
    }

    // If we don't match anything continue to the original handler, therefore crashing the app.
    if (previous_segv.sa_flags & SA_SIGINFO)
        previous_segv.sa_sigaction(signum, p_act, context);
    else
        previous_segv.sa_handler(signum);
}

HOOK_DEF(int, sigaction, int __signal, const struct sigaction* __new_action, struct sigaction* __old_action)
{
    if (__signal == SIGSEGV)
    {
        if (allow_call)
        {
            // If flag is set, then allow the call.
            allow_call = false;
            return orig_sigaction(__signal, __new_action, __old_action);
        }
        else
        {
            // This is where it prevents crashpad from overwriting the handler.
            dprint("something attempted to overwrite our handler");
            return 0;
        }
    }
    
    return orig_sigaction(__signal, __new_action, __old_action);
}

HOOK_DEF(JavaVM*, to_guest_jvm, JavaVM* jvm)
{
    std::call_once(copy_functions_jvm, [&]()
    {
        auto jvm_functions = reinterpret_cast<const unsigned long long*>(jvm->functions);
        for (auto i = 0; i < sizeof(JNIInvokeInterface) / 8; i++)
            jvm_vtable_copy[i] = jvm_functions[i];

        dprint("copied jvm functions");
    });

    if (std::find(known_vms.begin(), known_vms.end(), jvm) == known_vms.end())
    {
        // Found a new vm pointer, log it.
        known_vms.push_back(jvm);
        dprint("got jvm: %p", jvm);
    }

    return orig_to_guest_jvm(jvm);
}

HOOK_DEF(JNIEnv*, to_guest_jenv, JNIEnv* env)
{
    std::call_once(copy_functions_jenv, [&]()
    {
        auto jenv_functions = reinterpret_cast<const unsigned long long*>(env->functions);
        for (auto i = 0; i < sizeof(JNINativeInterface) / 8; i++)
            jenv_vtable_copy[i] = jenv_functions[i];

        dprint("copied jenv functions");
    });

    if (std::find(known_envs.begin(), known_envs.end(), env) == known_envs.end())
    {
        // Found a new environment pointer, log it.
        known_envs.push_back(env);
        dprint("got env: %p", env);
    }

    return orig_to_guest_jenv(env);
}

void* hook_loadLibraryExt(const char* lib_path, int flag, void* ns)
{
    if (strstr(lib_path, "libroblox.so"))
    {
        dprint("detected roblox");

        auto to_guest_jvm = DobbySymbolResolver("libndk_translation.so", "_ZN15ndk_translation13ToGuestJavaVMEPv");
        auto to_guest_jenv = DobbySymbolResolver("libndk_translation.so", "_ZN15ndk_translation13ToGuestJNIEnvEPv");
        
        // Hook both of these functions to intercept every JNIEnv and JavaVM objects the roblox native library has access to
        DobbyHook(to_guest_jvm, (void*)&new_to_guest_jvm, (void**)&orig_to_guest_jvm);
        DobbyHook(to_guest_jenv, (void*)&new_to_guest_jenv, (void**)&orig_to_guest_jenv);

        void *libc_sigaction = DobbySymbolResolver("libc.so", "sigaction");
        if (libc_sigaction)
        {
            // Hook sigaction to prevent crashpad in roblox from overwriting our handler
            // Don't stricly need to do this as I could just make it register after crashpad registers it's handler, but I am lazy.
            DobbyHook((void*)libc_sigaction, (void *) new_sigaction, (void **) &orig_sigaction);
            dprint("set sigaction hook");
        }

        dprint("setting sigsegv signal");

        // Register our segment violation handler
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_flags = SA_SIGINFO;
        sigfillset(&sa.sa_mask);
        sa.sa_sigaction = segv_handler;

        allow_call = true;
        sigaction(SIGSEGV, &sa, &previous_segv);
    }

    // Return back to the original function
    return orig_native_bridge->loadLibraryExt(lib_path, flag, ns);
}

void __attribute__ ((constructor)) setup()
{
    dprint("setup called");

    // Loads original ndk_translation into memory
    void* handle = dlopen("libndk_translation.so", RTLD_LAZY);
    if (handle != nullptr)
    {
        orig_native_bridge = reinterpret_cast<bridge_class*>(dlsym(handle, "NativeBridgeItf"));

        // Copies all the original function pointers to our struct so everything will function normally
        memcpy(&NativeBridgeItf, orig_native_bridge, sizeof(bridge_class));

        // We need to override the load library function to detect when roblox is loaded inside the app
        NativeBridgeItf.loadLibraryExt = &hook_loadLibraryExt;

        dprint("loaded libndk_translation and set functions");
    }
}