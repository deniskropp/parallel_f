// === (C) 2020 === parallel_f (tasks, queues, lists in parallel threads)
// Written by Denis Oliver Kropp <Leichenbegatter@outlook.com>

#include "parallel_f.hpp"


thread_local parallel_f::vthread* parallel_f::vthread::manager::current;

#if 0

//static void test_type();
static void test_tree();
static void test_var();
static void test_tt();


int main()
{
	test_tt();
	parallel_f::stats::instance::get().show_stats();
	return 0;

	auto task = parallel_f::make_task([]() {
			std::cout << "Hello World" << std::endl;
		});

	parallel_f::task_list tl;

	tl.append(task);
	tl.finish();

	parallel_f::stats::instance::get().show_stats();
}




namespace TT {

template <typename OT>
class ObjectType
{
public:
	static OT &Instance()
	{
		static OT instance;

		return instance;
	}

	static OT NewInstance()
	{
		return OT();
	}

private:
	std::map<std::string, std::pair<size_t,std::any>> fields;

public:
	template <typename T>
	ObjectType& AddField(std::string name, T default_value = T())
	{
		fields[name] = std::pair<size_t,std::any>(fields.size(), default_value);

		return *this;
	}

	const std::any& GetDefault(std::string name)
	{
		return fields[name].second;
	}

	size_t GetFieldCount()
	{
		return fields.size();
	}

	size_t GetFieldIndex(std::string name)
	{
		return fields[name].first;
	}

	const std::type_info& GetFieldType(std::string name)
	{
		return fields[name].second.type();
	}
};

template <typename OT>
class Object
{
protected:
	OT object_type;
	std::shared_ptr<std::vector<std::any>> object_data;

public:
	Object(OT& object_type)
		:
		object_type(object_type),
		object_data(std::make_shared<std::vector<std::any>>(object_type.GetFieldCount()))
	{
	}

	template <typename T>
	void Set(std::string name, T value)
	{
		//		parallel_f::logDebug("Object::Set('%s', %s) <- type.name: %s\n", name.c_str(), ustd::to_string(value).c_str(), typeid(T).name());
		parallel_f::logDebug("Object::Set('%s', %s) <- type.name: %s\n", name.c_str(), std::any(value).type().name(), typeid(T).name());

		if (object_type.GetFieldType(name) != typeid(T))
			throw std::runtime_error("invalid type argument");

		std::any& field_storage = object_data->at(object_type.GetFieldIndex(name));

		parallel_f::logDebug("   - <field_storage> <- type.name: %s\n", field_storage.type().name());

		if (field_storage.has_value() && field_storage.type() != typeid(T))
			throw std::runtime_error("mismatching field storage/argument types");

		std::any(value).swap(field_storage);
	}

	template <typename T>
	T Get(std::string name)
	{
		parallel_f::logDebug("Object::Get('%s') <- type.name: %s\n", name.c_str(), typeid(T).name());

		if (object_type.GetFieldType(name) != typeid(T))
			throw std::runtime_error("invalid type");

		const std::any& field_storage = object_data->at(object_type.GetFieldIndex(name));

		parallel_f::logDebug("   - <field_storage> <- type.name: %s\n", field_storage.type().name());

		if (field_storage.has_value())
			return std::any_cast<T>(field_storage);

		const std::any& default_value = object_type.GetDefault(name);

		parallel_f::logDebug("   - <default_value> <- type.name: %s\n", default_value.type().name());

		return std::any_cast<T>(default_value);
	}
};

template <typename OT>
auto make_object(OT& object_type = OT::Instance())
{
	return Object<OT>(object_type);
}

}



struct Vector2i
{
	int x;
	int y;

	Vector2i() : x(0), y(0) {}
	Vector2i(int x, int y) : x(x), y(y) {}
};

template <>
ustd::to_string<Vector2i>::to_string(const Vector2i& v)
{
	assign(ustd::to_string(v.x) + "," + ustd::to_string(v.y));
}


class TestObjectType : public TT::ObjectType<TestObjectType>
{
public:
	TestObjectType()
	{
		AddField<Vector2i>("position");
		AddField<ustd::string>("location");

		AddField<std::function<ustd::string(ustd::string)>>("encode", [](ustd::string text) { return text; });
	}
};

static void test_tt()
{
	parallel_f::setDebugLevel(1);
	parallel_f::system::instance().setAutoFlush();

//	auto test_object = TT::make_object<TestObjectType>();
	auto test_object = TT::make_object(TestObjectType::NewInstance()
											.AddField<float>("speed", 1.0f)
											.AddField<bool>("forward", true)
										);

	test_object.Set<Vector2i>("position", Vector2i(10, 20));
	test_object.Set<ustd::string>("location", "Berlin");
	test_object.Set<bool>("forward", false);

	parallel_f::logLine("position: ", test_object.Get<Vector2i>("position"));
	parallel_f::logLine("location: ", test_object.Get<ustd::string>("location"));
	parallel_f::logLine("speed: ", test_object.Get<float>("speed"));
	parallel_f::logLine("forward: ", test_object.Get<bool>("forward"));

	parallel_f::logLine("encode('test1234'): ", test_object.Get<std::function<ustd::string(ustd::string)>>("encode")("test1234"));

	test_object.Set<std::function<ustd::string(ustd::string)>>("encode", [](ustd::string text) { return text + ".XY"; });

	parallel_f::logLine("encode('test1234'): ", test_object.Get<std::function<ustd::string(ustd::string)>>("encode")("test1234"));
}




class Var
{
private:
	enum class type {
		none,
		number,
		string,
		boolean
	};

	type t;
	std::any v;

public:
	Var()
		:
		t(type::none)
	{
	}

	void setNumber(float number)
	{
		t = type::number;
		v = number;
	}

	void setString(std::string string)
	{
		t = type::string;
		v = string;
	}

	void setBoolean(bool boolean)
	{
		t = type::boolean;
		v = boolean;
	}

	bool isSet()
	{
		return t != type::none;
	}

	float asNumber()
	{
		switch (t) {
		case type::number:
			return std::any_cast<float>(v);
		case type::string:
			return (float)std::atof(std::any_cast<std::string>(v).c_str());
		case type::boolean:
			return std::any_cast<bool>(v) ? 1.0f : 0.0f;
		default:
			throw std::runtime_error("invalid type");
		}
	}

	std::string asString()
	{
		switch (t) {
		case type::number:
			return std::to_string(std::any_cast<float>(v));
		case type::string:
			return std::any_cast<std::string>(v);
		case type::boolean:
			return std::any_cast<bool>(v) ? "true" : "false";
		default:
			throw std::runtime_error("invalid type");
		}
	}

	bool asBoolean()
	{
		switch (t) {
		case type::number:
			return std::any_cast<float>(v) != 0.0f;
		case type::string: {
			std::string s = std::any_cast<std::string>(v);

			if (s == "true")
				return true;

			return asNumber() != 0.0f;
		}
		case type::boolean:
			return std::any_cast<bool>(v);
		default:
			throw std::runtime_error("invalid type");
		}
	}
};

static void test_var()
{
	Var v1, v2, v3;

	v1.setNumber(3);
	v2.setString("1");
	v3.setBoolean(true);

	for (int i = 0; i < 10; i++) {
		std::string str;
		std::cin >> str;
		v1.setString(str);

		std::cout << "v1.asNumber: " << v1.asNumber() << std::endl;
		std::cout << "v1.asString: " << v1.asString() << std::endl;
		std::cout << "v1.asBoolean: " << v1.asBoolean() << std::endl;
	}

	return;

	std::cout << "v2.asNumber: " << v2.asNumber() << std::endl;
	std::cout << "v2.asString: " << v2.asString() << std::endl;
	std::cout << "v2.asBoolean: " << v2.asBoolean() << std::endl;

	std::cout << "v3.asNumber: " << v3.asNumber() << std::endl;
	std::cout << "v3.asString: " << v3.asString() << std::endl;
	std::cout << "v3.asBoolean: " << v3.asBoolean() << std::endl;
}




class Node
{
private:
	Node* parent;
	std::list<Node*> children;

	void AddChild(Node* child)
	{
		children.push_back(child);
	}

public:
	Node(Node* parent = 0)
		:
		parent(parent)
	{
		if (parent)
			parent->AddChild(this);
	}

	size_t GetChildCount() const
	{
		return children.size();
	}
};

static void test_tree()
{
	//parallel_f::setDebugLevel(1);


	auto root = []() {
		return new Node();
	};

	auto node = [](auto parent) {
		Node *node = new Node( parent.get<Node*>() );

		return node;
	};

	auto task_root = parallel_f::make_task(root);
	auto task_n1 = parallel_f::make_task(node, task_root->result());
	auto task_n2 = parallel_f::make_task(node, task_root->result());
	auto task_n3 = parallel_f::make_task(node, task_n1->result());
	auto task_n4 = parallel_f::make_task(node, task_n1->result());
	auto task_n5 = parallel_f::make_task(node, task_n1->result());
	auto task_n6 = parallel_f::make_task(node, task_n2->result());
	auto task_n7 = parallel_f::make_task(node, task_n2->result());
	auto task_n8 = parallel_f::make_task(node, task_n2->result());

	parallel_f::task_list tl;

	auto id_root = tl.append(task_root);
	auto id_n1 = tl.append(task_n1, id_root);
	auto id_n2 = tl.append(task_n2, id_root);
	auto id_n3 = tl.append(task_n3, id_n1);
	auto id_n4 = tl.append(task_n4, id_n1);
	auto id_n5 = tl.append(task_n5, id_n1);
	auto id_n6 = tl.append(task_n6, id_n2);
	auto id_n7 = tl.append(task_n7, id_n2);
	auto id_n8 = tl.append(task_n8, id_n2);

	tl.finish();

	parallel_f::logInfoF("Root has %zu children.\n", task_root->result().get<Node*>()->GetChildCount());
}


/*

class TypeBase
{
public:
	virtual std::string GetTypeName() const = 0;
	virtual long long GetTypeID() = 0;

private:
	class Core
	{
	public:
		long long ids;

	public:
		Core() : ids(0) {}

		long long NextID()
		{
			return ++ids;
		}

		static Core& Instance()
		{
			static Core core;

			return core;
		}
	};

protected:
	template <typename T>
	static long long GetID()
	{
		static long long ID = Core::Instance().NextID();

		return ID;
	}

public:
	class Info
	{
	public:
		std::list<TypeBase*> members;

	public:
		template <typename M>
		Info Method(M m);
	};
};

template <typename T>
class Type : public TypeBase
{
public:
	virtual std::string GetTypeName() const
	{
		return typeid(T).name();
	}

	virtual long long GetTypeID()
	{
		return GetID<T>();
	}
};

template <typename M>
TypeBase::Info TypeBase::Info::Method(M m)
{
	members.push_back(new Type<M>());

	return *this;
}


class Test : public Type<Test>
{
public:
	void M1()
	{
	}

	void M2()
	{
	}
};

static Test::Info TestInfo = Test::Info().
							 Method(&Test::M1).
							 Method(&Test::M2);

class Test2 : public Type<Test2>
{

};


static void test_type()
{
	Test test;
	Test2 test2;

	parallel_f::logInfoF("TypeName '%s' (ID %lld)\n", test.GetTypeName().c_str(), test.GetTypeID());
	parallel_f::logInfoF("TypeName '%s' (ID %lld)\n", test2.GetTypeName().c_str(), test2.GetTypeID());

	for (auto m : TestInfo.members)
		parallel_f::logInfoF("TypeName '%s' (ID %lld)\n", m->GetTypeName().c_str(), m->GetTypeID());
}
*/

#endif