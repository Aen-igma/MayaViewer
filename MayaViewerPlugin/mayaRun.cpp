#include<maya/MItDependencyGraph.h>
#include"maya_includes.h"
#include<iostream>
#include<algorithm>
#include<vector>
#include<thread>
#include<queue>
#include<cmath>

#include"Vector.h"
#include"Matrix.h"
#include"EventHandler.h"

inline Mat4f& operator<< (Mat4f& m, const MMatrix& rhs) {
	rhs.get(m.mat);
	return m;
}

inline Vec4f& operator<< (Vec4f& v, const MColor& rhs) {
	v.x = rhs.r;
	v.y = rhs.g;
	v.z = rhs.b;
	v.w = rhs.a;
	return v;
}

inline Vec3f& operator<< (Vec3f& v, const MColor& rhs) {
	v.x = rhs.r;
	v.y = rhs.g;
	v.z = rhs.b;
	return v;
}

inline Vec3f& operator<< (Vec3f& v, const MPoint& rhs) {
	v.x = (float)rhs.x;
	v.y = (float)rhs.y;
	v.z = (float)rhs.z;
	return v;
}

inline Vec3f& operator<< (Vec3f& v, const MVector& rhs) {
	v.x = (float)rhs.x;
	v.y = (float)rhs.y;
	v.z = (float)rhs.z;
	return v;
}

struct Vertex {
	Vec3f position;
	Vec3f normal;
	Vec2f texcoord;

	Vertex() : position(), normal(), texcoord() {}

};


class CallbackHandler {
	public:
	CallbackHandler():callbacks() {}

	void Append(const std::string& node, const std::string& callbackName, MCallbackId callback) {
		if(!callbacks.count(node))
			callbacks.emplace(node, std::map<std::string, MCallbackId>());

		if(!callbacks.at(node).count(callbackName))
			callbacks.at(node).emplace(callbackName, callback);
		else
			MMessage::removeCallback(callback);
	}

	void RemoveCallback(const std::string& node, const std::string& callbackName) {
		if(callbacks.count(node))
			if(callbacks.at(node).count(callbackName)) {
				MMessage::removeCallback(callbacks.at(node).at(callbackName));
				callbacks.at(node).erase(callbackName);
			}
	}

	void RemoveAscociatedCallbacks(const std::string& node) {
		if(callbacks.count(node)) {
			for(auto& [key, i] : callbacks.at(node))
				MMessage::removeCallback(i);

			callbacks.erase(node);
		}
	}

	void ChangeNodeName(const std::string& oldName, const std::string& newName) {
		if(callbacks.count(oldName)) {
			std::map<std::string, MCallbackId> temp(callbacks.at(oldName));
			callbacks.erase(oldName);
			callbacks.emplace(newName, temp);
		}
	}

	void RemoveAllCallbacks() {
		for(auto& [key, i] : callbacks)
			for(auto& [key1, j] : i)
				MMessage::removeCallback(j);

		callbacks.clear();
	}

	private:
	std::map<std::string, std::map<std::string, MCallbackId>> callbacks;
};

CallbackHandler callbackHandler;
MCallbackId addNodeCallback;
MCallbackId connectionCallback;


MObject m_node;
MStatus status = MS::kSuccess;
bool initBool = false;

enum NODE_TYPE {TRANSFORM, MESH};


bool endThread(false);
std::thread update;
const size_t megaByte(1024000ull);
Comlib com(L"MayaViewer", megaByte * 500ull, ProcessType::Producer);
Comlib comRefresh(L"RefreshPlugin", megaByte, ProcessType::Consumer);
MessageHeader messageHeader;


// Send message to circular buffer

void SendMsg(void* msg, size_t size) {
	messageHeader.messageLength = size;
	for(uint32_t i = 0u; i < 100u; i++)
		if(com.Send(msg, &messageHeader)) break;
}

// Pre Defined Callback Functions
void TopologyModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData);
void PreTopologyIDModified(MUintArray componentIds[], unsigned int count, void* clientData);
void PreTopologyModified(MObject& node, void* clientData);
void VertexModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData);
void ShaderChanged(MPlug& srcPlug, MPlug& destPlug, bool made, void* clientData);
void ShaderModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData);
void PostNodeAdded(void* clientData);
void NameChanged(MObject& node, const MString& str, void* clientData);
void ObjectMoved(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData);
void NodeRemoved(MObject& node, void* clientData);
void NodeAdded(MObject& node, void* clientData);


// General Functions

uint32_t MStrLength(const MString& name) {
	return (name.length() > 50u) ? 50u : name.length();
}

uint32_t MFileLength(const MString& filePath) {
	return (filePath.length() > 150u) ? 150u : filePath.length();
}

void GetTexture(MObject& shader, MString& diffuseFilePath, MString& normalFilePath) {
	MPlug colorPlug = MFnDependencyNode(shader).findPlug("color");
	MItDependencyGraph itDiffuseGraph(colorPlug, MFn::kFileTexture,
								MItDependencyGraph::kUpstream,
								MItDependencyGraph::kBreadthFirst,
								MItDependencyGraph::kNodeLevel);


	itDiffuseGraph.disablePruningOnFilter();

	MObject diffuse = itDiffuseGraph.currentItem();
	MPlug diffuseFilePlug = MFnDependencyNode(diffuse).findPlug("fileTextureName");
	diffuseFilePlug.getValue(diffuseFilePath);



	MPlug normalPlug = MFnDependencyNode(shader).findPlug("normalCamera");
	MItDependencyGraph itNormalGraph(normalPlug, MFn::kBump,
								MItDependencyGraph::kUpstream,
								MItDependencyGraph::kBreadthFirst,
								MItDependencyGraph::kNodeLevel);
	
	itNormalGraph.disablePruningOnFilter();

	MObject bump = itNormalGraph.currentItem();
	MPlug bumpPlug = MFnDependencyNode(bump).findPlug("bumpValue");
	MItDependencyGraph itBumpGraph(bumpPlug, MFn::kFileTexture,
								MItDependencyGraph::kUpstream,
								MItDependencyGraph::kBreadthFirst,
								MItDependencyGraph::kNodeLevel);

	itBumpGraph.disablePruningOnFilter();

	MObject normal = itBumpGraph.currentItem();
	MPlug normalFilePlug = MFnDependencyNode(normal).findPlug("fileTextureName");
	normalFilePlug.getValue(normalFilePath);
}

void GetShaderData(MObject& node, MString& name, MColor& color, MColor& ambientColor) {
	switch(node.apiType()) {
		case MFn::kLambert: {
			MFnLambertShader shader(node);
			name = shader.name();
			color = shader.color();
			ambientColor = shader.ambientColor();

		} break;
		case MFn::kBlinn: {
			MFnBlinnShader shader(node);
			name = shader.name();
			color = shader.color();
			ambientColor = shader.ambientColor();
		} break;
		case MFn::kPhong: {
			MFnPhongShader shader(node);
			name = shader.name();
			color = shader.color();
			ambientColor = shader.ambientColor();
		} break;
	}
}

const MMatrix GetWorldMatrix(const MFnDagNode& dNode) {
	MMatrix parentMatrix = MMatrix::identity;
	if(dNode.parentCount() > 0)
		parentMatrix = GetWorldMatrix(dNode.parent(0));

	MTransformationMatrix tMatrix(MFnTransform(dNode.object()).transformation());
	return tMatrix.asMatrix() * parentMatrix;
}

void SetPos(const MObject& node, const bool& isCamera = false) {
	MFnDagNode dNode(node);
	MFnDependencyNode dNodeChild(MObject(dNode.child(0)));

	float orthoWidth(10.f);
	if(isCamera) {
		if(node.hasFn(MFn::kTransform)) {
			MObject child(dNode.child(0));
			MFnCamera camera(child);
			orthoWidth = static_cast<float>(camera.orthoWidth());
		} else {
			MFnCamera camera(node);
			orthoWidth = static_cast<float>(camera.orthoWidth());
		}
	} 

	EventTransform e;
	memcpy(e.name, dNodeChild.name().asChar(), MStrLength(dNodeChild.name()));
	e.isCamera = isCamera;
	e.orthoWidth = orthoWidth;
	e.transform << GetWorldMatrix(dNode);
	SendMsg(&e, sizeof(e));
}

void UpdateChildrenPos(const MObject& node) {
	MFnDagNode dNode(node);
	for(uint32_t i = 0u; i < dNode.childCount(); i++) {
		MObject nodeChild(dNode.child(i));
		if(nodeChild.hasFn(MFn::kTransform)) {
			SetPos(nodeChild);
			UpdateChildrenPos(nodeChild);
		}
	}
}

void GetMeshData(MObject& node, std::vector<Vertex>& vertecies) {
	
	MFnMesh mesh(node);
	uint32_t faceCount = mesh.numPolygons();

	for(uint32_t i = 0u; i < faceCount; i++) {

		int ind[3];
		MStatus s1 = mesh.getPolygonTriangleVertices(i, 0u, ind);
		MStatus s2 = mesh.getPolygonTriangleVertices(i, 1u, ind);

		for(uint32_t j = 0u; j < 2u; j++) {

			status = mesh.getPolygonTriangleVertices(i, j, ind);

			if(!status.error()) {
				for(uint32_t k = 0u; k < 3u; k++) {
					// Position
					MPoint pos;
					mesh.getPoint(ind[k], pos);

					// Normal
					MVector normal;
					mesh.getFaceVertexNormal(i, ind[k], normal);

					Vertex v;
					v.position << pos;
					v.normal << normal;

					// Texcoord
					if(!s1.error() && s2.error())
						mesh.getPolygonUV(i, k, v.texcoord.x, v.texcoord.y); // If single triangle, UV Order: 0, 1, 2
					else if(!j)
						mesh.getPolygonUV(i, (k > 1u) ? 3 : k, v.texcoord.x, v.texcoord.y); // If two triangle, first triangle UV Order: 0, 1, 3
					else
						mesh.getPolygonUV(i, (!k) ? 3u : k, v.texcoord.x, v.texcoord.y); // If two triangle, second triangle UV Order: 3, 1, 2

					vertecies.emplace_back(v);
				}
			}
		}
	}
}

bool AddMesh(MObject& node) {

	MStatus status;
	MFnMesh mesh(node);
	MFnDagNode dNode(node);

	// Mesh
	std::vector<Vertex> vertecies;
	GetMeshData(node, vertecies);

	// Material
	MString shaderName;
	MString textureFilePath;
	MString normalFilePath;
	MColor color;
	MColor ambientColor;

	MObjectArray shaderEngines;
	MIntArray shaderIndecies;
	mesh.getConnectedShaders(0u, shaderEngines, shaderIndecies);
	for(auto& i : shaderEngines) {
		MPlug surface = MFnDependencyNode(i).findPlug("surfaceShader");

		MObject shader;
		MPlugArray srcPlugs;
		surface.connectedTo(srcPlugs, true, false);
		if(srcPlugs.length() > 0u) shader = srcPlugs[0].node();
		
		GetTexture(shader, textureFilePath, normalFilePath);
		GetShaderData(shader, shaderName, color, ambientColor);
	}


	if(vertecies.size()) {
		EventMeshCreated e;
		memcpy(e.name, dNode.name().asChar(), MStrLength(dNode.name()));
		memcpy(e.shaderName, shaderName.asChar(), MStrLength(shaderName));
		memcpy(e.textureFilePath, textureFilePath.asChar(), MFileLength(textureFilePath));
		memcpy(e.normalFilePath, normalFilePath.asChar(), MFileLength(normalFilePath));
		e.color << color;
		e.ambientColor << ambientColor;
		e.vertexCount = static_cast<uint32_t>(vertecies.size());

		uint32_t eventSize = sizeof(EventMeshCreated);
		uint32_t vertSize = sizeof(Vertex) * e.vertexCount;
		uint32_t fullSize = eventSize + vertSize;
		char* data = NEW char[fullSize];
		memcpy(data, &e, eventSize);
		memcpy(data + sizeof(e), vertecies.data(), vertSize);
		SendMsg(data, fullSize);
		delete[] data;

		return true;
	}

	return false;
}


// Callback functions

void TopologyModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
	MObject node(plug.node());
	MFnDagNode dNode(node);

	if(msg & MNodeMessage::AttributeMessage::kAttributeEval) {

		std::vector<Vertex> vertecies;
		GetMeshData(node, vertecies);

		EventTopologyModified e;
		memcpy(e.name, dNode.name().asChar(), MStrLength(dNode.name()));
		e.vertexCount = static_cast<uint32_t>(vertecies.size());

		uint32_t eventSize = sizeof(EventTopologyModified);
		uint32_t vertSize = sizeof(Vertex) * e.vertexCount;
		uint32_t fullSize = eventSize + vertSize;
		char* data = NEW char[fullSize];
		memcpy(data, &e, eventSize);
		memcpy(data + sizeof(e), vertecies.data(), vertSize);
		SendMsg(data, fullSize);
		delete[] data;
	}

	callbackHandler.RemoveCallback((char*)clientData, "TopologyModified");
}

void PreTopologyIDModified(MUintArray componentIds[], unsigned int count, void* clientData) {
	MString name((char*)clientData);

	MSelectionList list;
	list.add(name);
	MObject node;
	list.getDependNode(0, node);

	MFnDagNode dNode(node);
	callbackHandler.Append(dNode.name().asChar(), "TopologyModified", MNodeMessage::addAttributeChangedCallback(node, TopologyModified, (void*)dNode.name().asChar()));
}

void PreTopologyModified(MObject& node, void* clientData) {
	MFnDagNode dNode(node);
	callbackHandler.Append(dNode.name().asChar(), "TopologyModified", MNodeMessage::addAttributeChangedCallback(node, TopologyModified, (void*)dNode.name().asChar()));
}

void VertexModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
	if(msg & MNodeMessage::AttributeMessage::kAttributeEval) {
		MObject node(plug.node());
		MFnDagNode dNode(node);

		std::vector<Vertex> vertecies;
		GetMeshData(node, vertecies);

		EventVertexModified e;
		memcpy(e.name, dNode.name().asChar(), MStrLength(dNode.name()));
		e.vertexCount = static_cast<uint32_t>(vertecies.size());

		uint32_t eventSize = sizeof(EventVertexModified);
		uint32_t vertSize = sizeof(Vertex) * e.vertexCount;
		uint32_t fullSize = eventSize + vertSize;
		char* data = NEW char[fullSize];
		memcpy(data, &e, eventSize);
		memcpy(data + sizeof(e), vertecies.data(), vertSize);
		SendMsg(data, fullSize);
		delete[] data;
	}
}

void ShaderChanged(MPlug& srcPlug, MPlug& destPlug, bool made, void* clientData) {
	MObject srcNode(srcPlug.node());
	MObject destNode(destPlug.node());
	MFnDependencyNode dSrcNode(srcNode);

	if(srcNode.hasFn(MFn::kMesh) && destNode.hasFn(MFn::kShadingEngine) && made) {
		MPlug surface = MFnDependencyNode(destNode).findPlug("surfaceShader");

		MObject shader;
		MPlugArray srcPlugs;
		surface.connectedTo(srcPlugs, true, false);
		if(srcPlugs.length() > 0u) shader = srcPlugs[0].node();

		MColor color;
		MString textureFilePath;
		MString normalFilePath;
		MColor ambientColor;
		MString shaderName;
		GetTexture(shader, textureFilePath, normalFilePath);
		GetShaderData(shader, shaderName, color, ambientColor);

		EventMaterialChanged e;
		memcpy(e.newName, shaderName.asChar(), MStrLength(shaderName));
		memcpy(e.meshName, dSrcNode.name().asChar(), MStrLength(dSrcNode.name()));
		memcpy(e.textureFilePath, textureFilePath.asChar(), MFileLength(textureFilePath));
		memcpy(e.normalFilePath, normalFilePath.asChar(), MFileLength(normalFilePath));
		e.color << color;
		e.ambientColor << ambientColor;
		SendMsg(&e, sizeof(e));
	}
}

void ShaderModified(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
	if(msg & MNodeMessage::AttributeMessage::kAttributeSet) {
		MObject node(plug.node());

		MString name;
		MString textureFilePath;
		MString normalFilePath;
		MColor color;
		MColor ambientColor;

		GetTexture(node, textureFilePath, normalFilePath);
		GetShaderData(node, name, color, ambientColor);

		EventMaterialModified e;
		memcpy(e.name, name.asChar(), MStrLength(name));
		memcpy(e.textureFilePath, textureFilePath.asChar(), MFileLength(textureFilePath));
		memcpy(e.normalFilePath, normalFilePath.asChar(), MFileLength(normalFilePath));
		e.color << color;
		e.ambientColor << ambientColor;
		SendMsg(&e, sizeof(e));

		Print("Msg:{0} | {1} \n", node.apiTypeStr(), name);
	}

}

void PostNodeAdded(void* clientData) {
	const char* name = (char*)clientData;

	MObject node;
	MItDependencyNodes it(MFn::kMesh);
	while(!it.isDone()) {
		MFnDagNode dCurrent(it.thisNode());
		if(dCurrent.name() == MString(name)) {
			node = it.thisNode();
			break;
		}
		it.next();
	}

	if(!node.hasFn(MFn::kInvalid))
		AddMesh(node);

	callbackHandler.RemoveCallback(name, "PostNodeAdded");
}

void NameChanged(MObject& node, const MString& str, void* clientData) {
	MFnDependencyNode dNode(node);

	EventNameChanged e;
	memcpy(e.newName, dNode.name().asChar(), MStrLength(dNode.name()));
	memcpy(e.previousName, str.asChar(), MStrLength(str));
	SendMsg(&e, sizeof(e));

	callbackHandler.ChangeNodeName(str.asChar(), dNode.name().asChar());
}

void ObjectMoved(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
	if(msg & MNodeMessage::AttributeMessage::kAttributeSet) {
		bool isCamera = clientData;
		SetPos(plug.node(), isCamera);
		UpdateChildrenPos(plug.node());
	}
}

void NodeRemoved(MObject& node, void* clientData) {
	MFnDependencyNode dNode(node);
	if(node.hasFn(MFn::kMesh)) {

		MString shaderName;
		MColor color;
		MColor ambientColor;

		MObjectArray shaderEngines;
		MIntArray shaderIndecies;
		MFnMesh(node).getConnectedShaders(0u, shaderEngines, shaderIndecies);
		for(auto& i : shaderEngines) {
			MPlug surface = MFnDependencyNode(i).findPlug("surfaceShader");

			MObject shader;
			MPlugArray srcPlugs;
			surface.connectedTo(srcPlugs, true, false);
			if(srcPlugs.length() > 0u) shader = srcPlugs[0].node();

			GetShaderData(shader, shaderName, color, ambientColor);
		}

		EventMeshDeleted e;
		memcpy(e.name, dNode.name().asChar(), MStrLength(dNode.name()));
		memcpy(e.shaderName, shaderName.asChar(), MStrLength(shaderName));
		SendMsg(&e, sizeof(e));
	}

	callbackHandler.RemoveAscociatedCallbacks(dNode.name().asChar());
}

void NodeAdded(MObject &node, void* clientData) {
	MFnDagNode dNode(node);

	if(node.hasFn(MFn::kTransform)) {
		callbackHandler.Append(dNode.name().asChar(), "ObjectMoved", MNodeMessage::addAttributeChangedCallback(node, ObjectMoved));
		callbackHandler.Append(dNode.name().asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(node, NodeRemoved));
	}

	if(node.hasFn(MFn::kLambert) || node.hasFn(MFn::kBlinn) || node.hasFn(MFn::kPhong)) {
		callbackHandler.Append(dNode.name().asChar(), "ShaderModified", MNodeMessage::addAttributeChangedCallback(node, ShaderModified));
		callbackHandler.Append(dNode.name().asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(node, NodeRemoved));
		callbackHandler.Append(dNode.name().asChar(), "NameChanged", MNodeMessage::addNameChangedCallback(node, NameChanged));
	}

	if(node.hasFn(MFn::kMesh)) {

		if(!AddMesh(node))
			callbackHandler.Append(dNode.name().asChar(), "PostNodeAdded", MEventMessage::addEventCallback("idle", PostNodeAdded, (void*)dNode.name().asChar()));

		bool want[3]{true};
		callbackHandler.Append(dNode.name().asChar(), "PreTopologyIDModified", MPolyMessage::addPolyComponentIdChangedCallback(node, want, 3, PreTopologyIDModified, (void*)dNode.name().asChar()));
		callbackHandler.Append(dNode.name().asChar(), "PreTopologyModified", MPolyMessage::addPolyTopologyChangedCallback(node, PreTopologyModified));
		callbackHandler.Append(dNode.name().asChar(), "VertexModified", MNodeMessage::addAttributeChangedCallback(node, VertexModified));
		callbackHandler.Append(dNode.name().asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(node, NodeRemoved));
		callbackHandler.Append(dNode.name().asChar(), "NameChanged", MNodeMessage::addNameChangedCallback(node, NameChanged));
	}
}


// Initialize

EXPORT MStatus initializePlugin(MObject obj) {

	MStatus res = MS::kSuccess;
	MFnPlugin plugin(obj, "level editor", "1.0", "Any", &res);

	if (MFAIL(res)) {
		CHECK_MSTATUS(res);
		return res;
	}

	std::cout.set_rdbuf(MStreamUtils::stdOutStream().rdbuf());
	std::cerr.set_rdbuf(MStreamUtils::stdErrorStream().rdbuf());

	Print("// -------------------------Plugin Loaded---------------------------- //\n");

	addNodeCallback = MDGMessage::addNodeAddedCallback(NodeAdded);
	connectionCallback = MDGMessage::addConnectionCallback(ShaderChanged);

	MItDependencyNodes itCam(MFn::kCamera);
	MObject mainCam(itCam.thisNode());
	while(!itCam.isDone()) {
		MObject cam(itCam.thisNode());
		MFnDagNode dagCam(cam);
		MObject camParent(dagCam.parent(0));
		const char* tag = "cam";
		callbackHandler.Append(dagCam.name().asChar(), "ObjectMoved", MNodeMessage::addAttributeChangedCallback(cam, ObjectMoved, (void*)tag));
		callbackHandler.Append(MFnDagNode(camParent).name().asChar(), "ObjectMoved", MNodeMessage::addAttributeChangedCallback(camParent, ObjectMoved, (void*)tag));
		itCam.next();
	}
	SetPos(MFnDagNode(mainCam).parent(0), true);


	MItDependencyNodes it(MFn::kMesh);
	while(!it.isDone()) {
		MObject node(it.thisNode());
		MFnDagNode dNode(node);

		AddMesh(node);
		bool want[3]{true};
		callbackHandler.Append(dNode.name().asChar(), "PreTopologyIDModified", MPolyMessage::addPolyComponentIdChangedCallback(node, want, 3, PreTopologyIDModified, (void*)dNode.name().asChar()));
		callbackHandler.Append(dNode.name().asChar(), "PreTopologyModified", MPolyMessage::addPolyTopologyChangedCallback(node, PreTopologyModified));
		callbackHandler.Append(dNode.name().asChar(), "VertexModified", MNodeMessage::addAttributeChangedCallback(node, VertexModified));
		callbackHandler.Append(dNode.name().asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(node, NodeRemoved));
		callbackHandler.Append(dNode.name().asChar(), "NameChanged", MNodeMessage::addNameChangedCallback(node, NameChanged));

		MObject parent(MFnDagNode(node).parent(0));
		MFnDagNode dParent(parent);
		SetPos(parent);
		UpdateChildrenPos(parent);

		if(parent.hasFn(MFn::kTransform)) {
			callbackHandler.Append(dParent.name().asChar(), "ObjectMoved", MNodeMessage::addAttributeChangedCallback(parent, ObjectMoved));
			callbackHandler.Append(dParent.name().asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(parent, NodeRemoved));
		}
		it.next();
	}


	std::function AddShaderModCallBack([](MItDependencyNodes& itShader) {
		while(!itShader.isDone()) {
			MObject shader(itShader.thisNode());
			MString name;
			
			switch(shader.apiType()) {
				case MFn::kLambert: {
					MFnLambertShader lambert(shader);
					name = lambert.name();
				} break;
				case MFn::kBlinn: {
					MFnBlinnShader blinn(shader);
					name = blinn.name();
				} break;
				case MFn::kPhong: {
					MFnPhongShader phong(shader);
					name = phong.name();
				} break;
			}

			callbackHandler.Append(name.asChar(), "ShaderModified", MNodeMessage::addAttributeChangedCallback(shader, ShaderModified));
			callbackHandler.Append(name.asChar(), "NodeRemoved", MNodeMessage::addNodePreRemovalCallback(shader, NodeRemoved));
			callbackHandler.Append(name.asChar(), "NameChanged", MNodeMessage::addNameChangedCallback(shader, NameChanged));
			itShader.next();
		}
	});

	MItDependencyNodes itLambert(MFn::kLambert);
	MItDependencyNodes itBlinn(MFn::kBlinn);
	MItDependencyNodes itPhong(MFn::kPhong);

	AddShaderModCallBack(itLambert);
	AddShaderModCallBack(itBlinn);
	AddShaderModCallBack(itPhong);


	update = std::thread([&]() {
		while(!endThread) {
			Event* event(nullptr);
			comRefresh.Inject((void**)&event);
			if(event) {
				EventDispatcher refresh(event);
				Sleep(100);
				refresh.Dispatch<EventRefreshPlugin>([&](EventRefreshPlugin& e) {
					MGlobal::executeCommandOnIdle(
						"unloadPlugin \"MayaViewerPlugin.mll\";"
						#ifndef _DEBUG
						"loadPlugin \"MayaViewerPlugin.mll\";");
						#else
						//Filepath to output directory used for debuging purposes.
						"loadPlugin \"C:/Users/maffi/Desktop/MayaViewer/bin/Debug-x64/MayaViewerPlugin.mll\";"); 
						#endif
				});
			}
		}
	});

	return res;
}


// UnInitialize

EXPORT MStatus uninitializePlugin(MObject obj) {
	MFnPlugin plugin(obj);

	Print("// ------------------------Plugin Unloaded--------------------------- //\n");

	endThread = true;
	update.join();

	callbackHandler.RemoveAllCallbacks();
	MMessage::removeCallback(addNodeCallback);
	MMessage::removeCallback(connectionCallback);

	return MS::kSuccess;
}