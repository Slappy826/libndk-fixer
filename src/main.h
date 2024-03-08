#pragma once

#include <android/log.h>
#include <unordered_map>
#include <iostream>
#include <dlfcn.h>
#include <jni.h>
#include <vector>

#include <dobby.h>

#define dprint(...) __android_log_print(ANDROID_LOG_DEBUG, "libndk-fix", __VA_ARGS__)
#define HOOK_DEF(ret, func, ...) \
  ret (*orig_##func)(__VA_ARGS__); \
  ret new_##func(__VA_ARGS__)

struct bridge_class
{
  uint32_t version;
  uint8_t _pad1[104];
  void* (*loadLibraryExt)(const char* lib_path, int flag, void* ns);
  uint8_t _pad2[28];
};