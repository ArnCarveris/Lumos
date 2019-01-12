#include "JM.h"
#include "VFS.h"

#include "FileSystem.h"


namespace jm 
{

	VFS* VFS::s_Instance = nullptr;

	void VFS::Init()
	{
		s_Instance = new VFS();
	}

	void VFS::Shutdown()
	{
		delete s_Instance;
	}

	void VFS::Mount(const String& virtualPath, const String& physicalPath)
	{
		JM_CORE_ASSERT(s_Instance,"");
		m_MountPoints[virtualPath].push_back(physicalPath);
	}

	void VFS::Unmount(const String& path)
	{
		JM_CORE_ASSERT(s_Instance,"");
		m_MountPoints[path].clear();
	}

	bool VFS::ResolvePhysicalPath(const String& path, String& outPhysicalPath)
	{
		if (path[0] != '/')
		{
			outPhysicalPath = path;
			return FileSystem::FileExists(path);
		}

		std::vector<String> dirs = SplitString(path, '/');
		const String& virtualDir = dirs.front();

		if (m_MountPoints.find(virtualDir) == m_MountPoints.end() || m_MountPoints[virtualDir].empty())
        {
            outPhysicalPath = path;
            return FileSystem::FileExists(path);
        }

		const String remainder = path.substr(virtualDir.size() + 1, path.size() - virtualDir.size());
		for (const String& physicalPath : m_MountPoints[virtualDir])
		{
			const String newPath = physicalPath + remainder;
			if (FileSystem::FileExists(newPath))
			{
				outPhysicalPath = newPath;
				return true;
			}
		}
		return false;
	}

	byte* VFS::ReadFile(const String& path)
	{
		JM_CORE_ASSERT(s_Instance,"");
		String physicalPath;
		return ResolvePhysicalPath(path, physicalPath) ? FileSystem::ReadFile(physicalPath) : nullptr;
	}

	String VFS::ReadTextFile(const String& path)
	{
		JM_CORE_ASSERT(s_Instance,"");
		String physicalPath;
		return ResolvePhysicalPath(path, physicalPath) ? FileSystem::ReadTextFile(physicalPath) : nullptr;
	}

	bool VFS::WriteFile(const String& path, byte* buffer)
	{
		JM_CORE_ASSERT(s_Instance,"");
		String physicalPath;
		return ResolvePhysicalPath(path, physicalPath) ? FileSystem::WriteFile(physicalPath, buffer) : false;

	}

	bool VFS::WriteTextFile(const String& path, const String& text)
	{
		JM_CORE_ASSERT(s_Instance,"");
		String physicalPath;
		return ResolvePhysicalPath(path, physicalPath) ? FileSystem::WriteTextFile(physicalPath, text) : false;
	}

}