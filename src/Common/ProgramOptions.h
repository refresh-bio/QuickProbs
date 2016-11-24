#pragma once

#include <string>
#include <iostream>
#include <istream>
#include <sstream>
#include <vector>
#include <memory>
#include <list>
#include <map>
#include <algorithm>
#include <set>

class AbstractOption {
public:
	std::string getName() const { return name; }
	std::string getDescription() const { return desc; }
	bool getIsDeveloper() const { return isDeveloper; }

	AbstractOption(const std::string& name, const std::string& desc, bool isDeveloper) : name(name), desc(desc), isDeveloper(isDeveloper) {}

	virtual bool parse(const std::string& valstr) = 0;

protected:
	const std::string name;
	const std::string desc;
	const bool isDeveloper;
};

template <class T>
class Option : public AbstractOption {
public:
	bool getIsSet() const { return isSet; }
	
	Option(const std::string& name, const std::string& desc, bool isDeveloper)
		: AbstractOption(name, desc, isDeveloper), isSet(false) {}

	Option(const std::string& name, const std::string& desc, bool isDeveloper, const T def)
		: AbstractOption(name, desc, isDeveloper), value(def), isSet(true) {}

	T get() const { return value; }
	void set(T v) { this->value = v; isSet = true;  }

	virtual bool parse(const std::string& valstr) {
		std::istringstream iss(valstr);
		bool ok = static_cast<bool>(iss >> value);
		isSet |= ok;
		return ok;
	}

protected:
	T value;
	bool isSet ;
};

class Switch : public Option<bool>
{
public:
	Switch(const std::string& name, const std::string& desc, bool isDeveloper) : Option(name, desc, isDeveloper, false) {}
};


class ProgramOptions
{
public:
	ProgramOptions(const std::string & executable) : executable(executable)
	{

	}

	/// <summary>
	/// Adds option with given name and description.
	/// </summary>
	/// <param name ="name">Option name.</parameter>
	/// <param name ="desc">Option description.</parameter>
	template <typename Type>
	void add(const std::string & name, const std::string & desc, Type def, bool developer)
	{  
		size_t sepPos = name.find(',');
		std::string mainName = (sepPos == std::string::npos) ? name : name.substr(0,sepPos);
		std::string shortName = (sepPos == std::string::npos) ? "" : name.substr(sepPos + 1);

		auto option = std::shared_ptr<AbstractOption>(new Option<Type>(name, desc, developer, def));
		options[mainName] = option;
		if (shortName.length() > 0) {
			options[shortName] = option;
			shortNames.insert(shortName);
		}
	}

	template <typename Type>
	void add(const std::string & name, const std::string & desc, bool developer)
	{
		size_t sepPos = name.find(',');
		std::string mainName = (sepPos == std::string::npos) ? name : name.substr(0, sepPos);
		std::string shortName = (sepPos == std::string::npos) ? "" : name.substr(sepPos + 1);

		auto option = std::shared_ptr<AbstractOption>(new Option<Type>(name, desc, developer));
		options[mainName] = option;
		if (shortName.length() > 0) {
			options[shortName] = option;
			shortNames.insert(shortName);
		}
	}

	
	template <typename Type>
	void addPositional(const std::string & name, const std::string & desc)
	{
		auto option = std::shared_ptr<AbstractOption>(new Option<Type>(name, desc, false));
		positionalOptions.push_back(option);
	}

	void addSwitch(const std::string & name, const std::string & desc, bool developer)
	{
		size_t sepPos = name.find(',');
		std::string mainName = (sepPos == std::string::npos) ? name : name.substr(0, sepPos);
		std::string shortName = (sepPos == std::string::npos) ? "" : name.substr(sepPos + 1);

		auto option = std::shared_ptr<AbstractOption>(new Switch(name, desc, developer));
		options[mainName] = option;
		if (shortName.length() > 0) {
			options[shortName] = option;
			shortNames.insert(shortName);
		}
	}


	bool parse(int argc, char *argv[])
	{
		// copy parameters to temporary collection
		std::list<std::string> args(argc - 1);
		std::transform(argv + 1, argv + argc, args.begin(), [](char * a)->std::string { return std::string(a); });

		// parse normal options
		for (auto it = args.begin(); it != args.end(); ) {
			std::string param = *it;

			if (param[0] != '-') {
				++it;
				continue;
			}

			while (param[0] == '-') {
				param = param.substr(1);
			}
			
			if (options.find(param) != options.end()) {
				it = args.erase(it); // remove parsed option from collection
				auto option = options[param];

				// check if option is a switch
				auto switchOption = std::dynamic_pointer_cast<Switch>(option);
				if (switchOption) {
					switchOption->set(true);

				} else {
					const std::string& value = *it;
					if (option->parse(value)) {
						it = args.erase(it); // remove parsed value from collection
					} else {
						// unable to parse
						
					}
				}
			}
		}

		// everything left are positional options
		int posId = 0;
		
		for (auto it = args.begin(); it != args.end() && posId != positionalOptions.size(); ++posId, ++it) {
			std::string param = *it;
			positionalOptions[posId]->parse(param);
		}

		if (posId < positionalOptions.size()) {
			return false;
		}
		
		return true;
	}

	bool exists(const std::string & name) const
	{
		return options.find(name) != options.end();
	}

	template <typename T>
	bool get(const std::string & name, T &result)
	{
		auto it = options.find(name);
		if (it != options.end()) {
			auto option = dynamic_pointer_cast<Option<T>>(it->second);
			if (option->getIsSet()) {
				result = option->get();
				return true;
			}
		}

		auto it2 = std::find_if(positionalOptions.begin(), positionalOptions.end(), [&name](std::shared_ptr<AbstractOption>& o) { return o->getName() == name; });
		if (it2 != positionalOptions.end()) {
			auto option = dynamic_pointer_cast<Option<T>>(*it2);
			if (option->getIsSet()) {
				result = option->get();
				return true;
			}
		}
			
		return false;
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
		out << "Options:" << std::endl;
		for (const auto& o : options) {
			if (!o.second->getIsDeveloper() && shortNames.find(o.first) != shortNames.end()) {
				std::cout << o.second->getName() << ": " << o.second->getDescription() << std::endl;
			}
		}
		
		out << "Developer options:" << std::endl;

		if (showDeveloperOptions) {
			for (const auto& o : options) {
				if (o.second->getIsDeveloper()) {
					out << o.second->getName() << ": " << o.second->getDescription() << std::endl;
				}
			}
		}

		return out.str();
	}

private:
	std::map<std::string, std::shared_ptr<AbstractOption>> options;
	std::set<std::string> shortNames;
	
	std::vector<std::shared_ptr<AbstractOption>> positionalOptions;

	std::string executable;

};