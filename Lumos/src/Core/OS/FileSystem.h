#pragma once

#include "lmpch.h"

namespace Lumos
{
	class FileSystem
	{
	public:
		static bool FileExists(const String& path);
		static bool FolderExists(const String& path);
		static i64 GetFileSize(const String& path);

		static u8* ReadFile(const String& path);
		static bool ReadFile(const String& path, void* buffer, i64 size = -1);
		static String ReadTextFile(const String& path);

		static bool WriteFile(const String& path, u8* buffer);
		static bool WriteTextFile(const String& path, const String& text);

		//static void SetRootDirectory(const String& path);
	private:

		//static String s_RootDirectory;
	};

}
