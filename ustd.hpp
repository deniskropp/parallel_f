// === (C) 2020 === parallel_f / ustd (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#pragma once

#include <any>
#include <string>


// parallel_f :: ustd == implementation

namespace ustd {

class string
{
private:
	std::string str;

public:
	template <typename... Args>
	string(Args... args)
	{
		(str.append(args), ...);
	}

	std::string to_string() const
	{
		return std::string("'") + str + std::string("'");
	}

	void assign(const std::string& s)
	{
		str.assign(s);
	}

	operator std::string() const
	{
		return str;
	}

	string operator +(const string& other) const
	{
		return string(str, other.str);
	}
};


template <typename __Type>
class to_string : public string
{
public:
	to_string(const __Type& v)
	{
		std::string name(typeid(v).name());

		size_t off = 0;

		if (name.find(" ") != std::string::npos)
			off = name.find(" ") + 1;

		std::stringstream ss;

		ss << name.substr(off) << std::string("(") << v << std::string(")");

		assign(ss.str());
	}
};


template <>
to_string<ustd::string>::to_string(const ustd::string& v)
{
	assign(ustd::string("'") + v + ustd::string("'"));
}

template <>
to_string<std::any>::to_string(const std::any& v)
{
	std::string name(v.type().name());

	size_t off = 0;

	if (name.find(" ") != std::string::npos)
		off = name.find(" ") + 1;

	assign(name.substr(off) + std::string("(") + std::string(to_string(v.has_value())) + std::string(")"));
}

template <>
to_string<std::string>::to_string(const std::string& v)
{
	assign(std::string("'") + v + std::string("'"));
}

template <>
to_string<float>::to_string(const float& v)
{
	assign(std::to_string(v));
}

template <>
to_string<double>::to_string(const double& v)
{
	assign(std::to_string(v));
}

template <>
to_string<int>::to_string(const int& v)
{
	assign(std::to_string(v));
}

template <>
to_string<long int>::to_string(const long int& v)
{
	assign(std::to_string(v));
}

template <>
to_string<long long int>::to_string(const long long int& v)
{
	assign(std::to_string(v));
}

template <>
to_string<bool>::to_string(const bool& v)
{
	assign(v ? "true" : "false");
}


}
