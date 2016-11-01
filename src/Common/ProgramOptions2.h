#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>

class AbstractOption {
public:
	const std::string name;
	const std::string desc;
	const bool isDeveloper;
};

template <class T>
class Option : public IOption {
public:
	Option(
		const std::string& name,
		const std::string& desc,
		const T def,
		bool isDeveloper) 
		: name(name), desc(desc), def(def), isDeveloper(isDeveloper) {}
private:
	T default;
	T value;
	
};

class Switch : public IOption{
public:
	

	Switch(const std::string& name, const std::string& desc, bool isDeveloper) :
		name(name), desc(desc), isDeveloper(isDeveloper) {}
};

class ProgramOptions2
{
public:
	ProgramOptions2(const std::string & executable) : executable(executable)
	{

	}

	void addSwitch(const std::string & name, const std::string & desc, bool developer)
	{
		switches.emplace_back(name, desc, developer);
	}

	/// <summary>
	/// Adds option with given name and description.
	/// </summary>
	/// <param name ="name">Option name.</parameter>
	/// <param name ="desc">Option description.</parameter>
	template <typename Type>
	void add(const std::string & name, const std::string & desc, bool developer)
	{
		po::options_description& ops = developer ? developerOptions : normalOptions;
		ops.add_options()(name.c_str(), po::value<Type>(), desc.c_str());
	}

	/// <summary>
	/// Adds option with given name, description and default value.
	/// </summary>
	/// <param name ="name">Option name.</parameter>
	/// <param name ="desc">Option description.</parameter>
	/// <param name ="def"></parameter>
	/// <returns></returns>
	template <typename Type>
	void add(const std::string & name, const std::string & desc, Type def, bool developer)
	{
		po::options_description& ops = developer ? developerOptions : normalOptions;
		ops.add_options()(name.c_str(), po::value<Type>()->default_value(def), desc.c_str());
	}

	template <typename ElementType>
	void addVector(const std::string & name, const std::string & desc, bool developer)
	{
		po::options_description& ops = developer ? developerOptions : normalOptions;
		ops.add_options()(name.c_str(), po::value<std::vector<ElementType>>()->multitoken(), desc.c_str());
	}

	template <typename Type>
	void addPositional(const std::string & name, const std::string & desc)
	{
		normalOptions.add_options()(name.c_str(), po::value<Type>(), desc.c_str());
		positionalOptions.add(name.c_str(), 1);
	}

	template <typename Type>
	void addPositional(const std::string & name, const std::string & desc, Type def)
	{
		normalOptions.add_options()(name.c_str(), po::value<Type>()->default_value(def), desc.c_str());
		positionalOptions.add(name.c_str(), 1);
	}

	void parse(int argc, char *argv[])
	{
		
		
		
		std::vector<std::string> args;

		for (int i = 0; i < argc; i++)
			args.push_back(std::string(argv[i]));

		po::command_line_parser parser(argc, argv);
		//po::basic_command_line_parser<char> parser(args);
		po::options_description desc;
		desc.add(normalOptions).add(developerOptions);

		parser.options(desc);
		parser.positional(positionalOptions);

		//parser.allow_unregistered();
		store(parser.run(), map);
		notify(map);
	}

	const bool exists(const std::string & name)
	{
		return (map.count(name) > 0);
	}

	template <typename T>
	const bool get(const std::string & name, T &result)
	{
		if (map.count(name) == 0)
			return false;

		result = map[name].as<T>();
		return true;
	}

	template <typename T>
	T get(const std::string & name)
	{
		T var;
		if (!get(name, var)) {
			throw std::runtime_error("Unable to read parameter: " + name);
		}

		return var;
	}

	const std::string toString(bool showDeveloperOptions) const
	{
		std::ostringstream out;

		// print command line
		out << "Options:" << std::endl << normalOptions << std::endl;

		if (showDeveloperOptions) {
			out << std::endl << "Developer options:" << std::endl << developerOptions << std::endl;
		}

		return out.str();
	}

private:
	std::vector<std::shared_ptr<IOption>> normalOptions;
	std::vector<std::shared_ptr<IOption>> developerOptions;

	std::vector<Switch> switches;

	std::string executable;

	po::variables_map map;
	po::options_description normalOptions;
	po::options_description developerOptions;
	po::positional_options_description positionalOptions;


};