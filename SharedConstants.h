#pragma once
#include <tchar.h>
#include <windows.h>

static const TCHAR* kSemaphoreName = _T("Global\\DownloadSlots");
static const TCHAR* kMutexName = _T("Global\\LogAccessMutex");
static const TCHAR* kEventName = _T("Global\\BrowserClosingEvent");

static const int kMinDownloadDelay = 1;
static const int kMaxDownloadDelay = 3;
static const int kMockDataSize = 200;