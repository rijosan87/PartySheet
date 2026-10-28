#pragma once

#include <Windows.h>

namespace SettingsIni
{
    bool BeginLoad();
    void EndLoad();

    bool BeginSave();
    bool EndSave();

    UINT GetIntW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        INT a_default,
        const wchar_t* a_path);

    DWORD GetStringW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_default,
        wchar_t* a_buffer,
        DWORD a_bufferSize,
        const wchar_t* a_path);

    BOOL WriteStringW(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_value,
        const wchar_t* a_path);

    int ReadPresetIntOnce(
        const wchar_t* a_section,
        const wchar_t* a_key,
        int a_default);

    bool WritePresetStringOnce(
        const wchar_t* a_section,
        const wchar_t* a_key,
        const wchar_t* a_value);
}
