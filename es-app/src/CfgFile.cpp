#include "CfgFile.h"
#include <string>
#include <boost/filesystem.hpp>
#include <sstream>
#include <iostream>
#include <fstream>  
#include "EmulationStation.h"

bool StartsWith(const std::string& str, const std::string& prefix)
{
	if (str.substr(0, prefix.size()) == prefix)
	{
		return true;
	}
	return false;
}

CfgFile::CfgFile(const std::string& path)
	: m_path(path)
{
	LoadConfigFile();
}

void CfgFile::LoadConfigFile(const std::string path)
{
	if (boost::filesystem::exists(path))
	{
		m_path = path;
		m_cfgEntries.clear();
		m_cfgEntries.emplace_back("# Generated by " + std::string(PROGRAM_VERSION_STRING));

		std::ifstream is_file(path);
		std::string comment("#");

		std::string line;
		while (std::getline(is_file, line))
		{
			std::istringstream is_line(line);
			std::string key;
			if (!StartsWith(line, comment) && std::getline(is_line, key, '='))
			{
				std::string value;
				if (std::getline(is_line, value))
				{
					m_cfgEntries.emplace_back(key, value);

				}
			}
			else
			{
				m_cfgEntries.emplace_back(line);
			}
		}
	}
}

void CfgFile::LoadConfigFile()
{
	LoadConfigFile(m_path);
}

bool CfgFile::ConfigFileExists() const
{
	return boost::filesystem::exists(m_path);
}

void CfgFile::DeleteConfigFile() const
{
	if (ConfigFileExists())
	{
		boost::filesystem::remove(m_path);
	}
}

void CfgFile::SaveConfigFile() const
{
	if (!ConfigFileExists())
	{
		std::ofstream outfile(m_path);

		for (const CfgEntry& entry: m_cfgEntries)
		{
			outfile << entry.GetLine() << std::endl;
		}
		outfile.close();
	}
}


const std::string& CfgFile::GetConfigFilePath() const
{
	return m_path;
}

std::string CfgEntry::GetLine() const
{
	std::string result;
	if (_key.empty() || _value.empty())
	{
		result = _comment;
	}
	else
	{
		result =  _key + " = " + _value;
		if (!_comment.empty())
		{
			result += " " + _comment;
		}
	}
	return result;
}