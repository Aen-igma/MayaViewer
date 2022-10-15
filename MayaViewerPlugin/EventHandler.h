#pragma once
#include"ComLib/Comlib.h"
#include"maya_includes.h"
#include<functional>
#include"Matrix.h"

enum class EventType {
	None,
	RefreshPlugin,
	MeshCreated,
	MeshDeleted,
	NameChanged,
	Transform,
	MaterialModified,
	MaterialChanged,
	VertexModified,
	TopologyModified,
};

struct Event {
	Event() :type(EventType::None) {}
	Event(EventType eType) :type(eType) {}
	virtual ~Event() = 0;

	const EventType GetType() {
		return type;
	}

	EventType type;
};

inline Event::~Event() {}


struct EventRefreshPlugin : public Event {
	EventRefreshPlugin() :Event(EventType::RefreshPlugin) {}
	virtual ~EventRefreshPlugin() override {};

	static EventType GetStaticType() {
		return EventType::RefreshPlugin;
	}
};

struct EventMeshCreated : public Event {
	EventMeshCreated() :Event(EventType::MeshCreated), name{'\0'}, shaderName{'\0'}, textureFilePath{'\0'}, normalFilePath{'\0'}, 
		color(), ambientColor(), vertexCount(0u) {}
	virtual ~EventMeshCreated() override {};

	static EventType GetStaticType() {
		return EventType::MeshCreated;
	}

	char name[50];
	char shaderName[50];
	char textureFilePath[150];
	char normalFilePath[150];
	Vec4f color;
	Vec3f ambientColor;
	uint32_t vertexCount;
};

struct EventMeshDeleted : public Event {
	EventMeshDeleted() :Event(EventType::MeshDeleted), name{'\0'}, shaderName{'\0'} {}
	virtual ~EventMeshDeleted() override {};

	static EventType GetStaticType() {
		return EventType::MeshDeleted;
	}

	char name[50];
	char shaderName[50];
};

struct EventTransform : public Event {
	EventTransform() :Event(EventType::Transform), transform(), isCamera(false), orthoWidth(10.f), fov(45.f), name{'\0'} {}
	virtual ~EventTransform() override {};

	static EventType GetStaticType() {
		return EventType::Transform;
	}

	char name[50];
	bool isCamera;
	float orthoWidth;
	float fov;
	Mat4f transform;
};

struct EventNameChanged : public Event {
	EventNameChanged() :Event(EventType::NameChanged), newName{'\0'}, previousName{'\0'} {}
	virtual ~EventNameChanged() override {};

	static EventType GetStaticType() {
		return EventType::NameChanged;
	}

	char newName[50];
	char previousName[50];
};

struct EventMaterialModified : public Event {
	EventMaterialModified() :Event(EventType::MaterialModified), name{'\0'}, textureFilePath{'\0'}, normalFilePath{'\0'},
		color(), ambientColor() {}
	virtual ~EventMaterialModified() override {};

	static EventType GetStaticType() {
		return EventType::MaterialModified;
	}

	char name[50];
	char textureFilePath[150];
	char normalFilePath[150];
	Vec4f color;
	Vec3f ambientColor;
};

struct EventMaterialChanged : public Event {
	EventMaterialChanged() :Event(EventType::MaterialChanged), newName{'\0'}, meshName{'\0'}, textureFilePath{'\0'}, normalFilePath{'\0'},
		color(), ambientColor() {}
	virtual ~EventMaterialChanged() override {};

	static EventType GetStaticType() {
		return EventType::MaterialChanged;
	}

	char newName[50];
	char meshName[50];
	char textureFilePath[150];
	char normalFilePath[150];
	Vec4f color;
	Vec3f ambientColor;
};

struct EventVertexModified : public Event {
	EventVertexModified() :Event(EventType::VertexModified), name{'\0'}, vertexCount(0u) {}
	virtual ~EventVertexModified() override {};

	static EventType GetStaticType() {
		return EventType::VertexModified;
	}

	char name[50];
	uint32_t vertexCount;
};

struct EventTopologyModified : public Event {
	EventTopologyModified() :Event(EventType::TopologyModified), name{'\0'}, vertexCount(0u) {}
	virtual ~EventTopologyModified() override {};

	static EventType GetStaticType() {
		return EventType::TopologyModified;
	}

	char name[50];
	uint32_t vertexCount;
};


// Event Dispatcher

template<typename T>
using EventFunc = std::function<void(T&)>;

class EventDispatcher {
	public:

	EventDispatcher(Event* event)
		:m_event(event) {};

	template<typename T>
	bool Dispatch(EventFunc<T> e) {
		if(m_event->GetType() == T::GetStaticType()) {
			e(*(T*)m_event);
			return true;
		}
		return false;
	}

	private:
	Event* m_event;
};