#include "MayaViewer.h"
#include<thread>


// Declare our game instance
MayaViewer game;

const size_t megaByte(1024000ull);
Comlib com(L"MayaViewer", megaByte * 500ull, ProcessType::Consumer);
Comlib comRefresh(L"RefreshPlugin", megaByte, ProcessType::Producer);
MessageHeader messageHeader;
std::map<std::string, std::map<std::string, Material*>> materials;


struct Vertex {
	Vector3 position;
	Vector3 normal;
	Vector3 tangent;
	Vector3 biTangent;
	Vector2 texcoord;

	Vertex() : position(Vector3::zero()), normal(Vector3::zero()), texcoord(Vector2::zero()) {}
};

Mesh* CreateMesh(Vertex* meshData, uint32_t vertexCount) {
	VertexFormat::Element elements[] = {
		VertexFormat::Element(VertexFormat::POSITION, 3),
		VertexFormat::Element(VertexFormat::NORMAL, 3),
		VertexFormat::Element(VertexFormat::TANGENT, 3),
		VertexFormat::Element(VertexFormat::BINORMAL, 3),
		VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
	};
	Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 5), vertexCount, true);
	if(mesh == nullptr) {
		GP_ERROR("Failed to create mesh.");
		return nullptr;
	}
	mesh->setVertexData(meshData, 0, vertexCount);
	return mesh;
}


MayaViewer::MayaViewer()
    : _scene(NULL) {
}

void MayaViewer::initialize() {

	SET_DEBUG_FLAGS;

    _scene = Scene::create();
	
	Camera* camera = Camera::createPerspective(45.f, getAspectRatio(), 0.01f, 2000.f);
	Node* camNode = _scene->addNode("camera");
	camNode->setCamera(camera);
	_scene->setActiveCamera(camera);
	SAFE_RELEASE(camera);

	com.ClearMemory();

	EventRefreshPlugin e;
	messageHeader.messageLength = sizeof(e);
	comRefresh.Send(&e, &messageHeader);
}

void MayaViewer::finalize() {
    SAFE_RELEASE(_scene);
}

void MayaViewer::EventCallback(Event* event) {

	EventDispatcher addMesh(event);
	addMesh.Dispatch<EventMeshCreated>([&](EventMeshCreated& e) {
		Vertex* vertecies = NEW Vertex[e.vertexCount];
		memcpy(vertecies, &e + 1ull, sizeof(Vertex) * e.vertexCount);

		Mesh* mesh = CreateMesh(vertecies, e.vertexCount);
		Model* model = Model::create(mesh);
		
		Material* material = model->setMaterial("resource/shaders/textured.vert", "resource/shaders/Custom.frag", "BUMPED;DIRECTIONAL_LIGHT_COUNT 1");
		material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
		material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");
		material->getParameter("u_ambientColor")->setValue(e.ambientColor);
		material->getParameter("u_diffuseColor")->setValue(e.color);
		material->getParameter("u_directionalLightColor[0]")->setValue(Vector3(0.6f, 0.6f, 0.6f));
		material->getParameter("u_directionalLightDirection[0]")->setValue(Vector3(0.f, 0.f, -1.f));
		material->getStateBlock()->setCullFace(true);
		material->getStateBlock()->setDepthTest(true);

		Texture::Sampler* sampler;
		std::string filePath(e.textureFilePath);
		if(!filePath.empty())
			sampler = material->getParameter("u_diffuseTexture")->setValue(filePath.c_str(), true);
		else
			sampler = material->getParameter("u_diffuseTexture")->setValue("resource/DefaultTexture.png", true);
		sampler->setFilterMode(Texture::NEAREST_MIPMAP_LINEAR, Texture::LINEAR);
		sampler->setWrapMode(Texture::Wrap::REPEAT, Texture::Wrap::REPEAT);

		filePath = e.normalFilePath;
		if(!filePath.empty())
			sampler = material->getParameter("u_normalmapTexture")->setValue(filePath.c_str(), true);
		else
			sampler = material->getParameter("u_normalmapTexture")->setValue("resource/DefaultNormal.png", true);
		sampler->setFilterMode(Texture::NEAREST_MIPMAP_LINEAR, Texture::LINEAR);
		sampler->setWrapMode(Texture::Wrap::REPEAT, Texture::Wrap::REPEAT);


		if(!materials.count(e.shaderName))
			materials.emplace(e.shaderName, std::map<std::string, Material*>());

		materials.at(e.shaderName).emplace(e.name, material);

		std::string name(e.name);
		Node* node = _scene->addNode(name.c_str());
		node->setDrawable(model);
		node->translate(0.f, 0.f, 0.f);
		SAFE_RELEASE(model);
		SAFE_RELEASE(mesh);

		delete[] vertecies;
	});

	EventDispatcher removeMesh(event);
	removeMesh.Dispatch<EventMeshDeleted>([&](EventMeshDeleted& e) {
		Node* node = _scene->findNode(e.name);
		if(node) {
			_scene->removeNode(node);
			materials.at(e.shaderName).erase(e.name);
		}
	});

	EventDispatcher nameChanged(event);
	nameChanged.Dispatch<EventNameChanged>([&](EventNameChanged& e) {
		Node* node = _scene->findNode(e.previousName);
		if(node) {
			node->setId(e.newName);

			std::string materialName;
			Material* material(nullptr);

			for(auto& [key, i] : materials)
				if(i.count(e.previousName)) {
					materialName = key;
					material = i.at(e.previousName);
					i.erase(e.previousName);
					break;
				}

			if(material)
				materials.at(materialName).emplace(e.newName, material);
		}
	});

	EventDispatcher objectMoved(event);
	objectMoved.Dispatch<EventTransform>([&](EventTransform& e) {
		std::string name(e.name);
		Node* node = _scene->findNode(name.c_str());
		Vector3 translation;
		Quaternion rotation;
		Vector3 scale;
		if(node) {
			e.transform.getTranslation(&translation);
			e.transform.getRotation(&rotation);
			e.transform.getScale(&scale);
			node->setTranslation(translation);
			node->setRotation(rotation);
			node->setScale(scale);
		} else if(e.isCamera) {
			Camera* cam = _scene->getActiveCamera();
			Matrix proj;
			float fov = MATH_RAD_TO_DEG(e.fov);
			if(name.find("persp") != std::string::npos)
				Matrix::createPerspective(fov, getAspectRatio(), 0.01f, 2000.f, &proj);
			else
				Matrix::createOrthographic(e.orthoWidth, e.orthoWidth / getAspectRatio(), 0.01f, 2000.f, &proj);
			cam->setProjectionMatrix(proj);

			Node* camNode = cam->getNode();
			e.transform.getTranslation(&translation);
			e.transform.getRotation(&rotation);
			camNode->setTranslation(translation);
			camNode->setRotation(rotation);
		}
	});

	EventDispatcher materialModified(event);
	materialModified.Dispatch<EventMaterialModified>([&](EventMaterialModified& e) {
		if(materials.count(e.name)) {
			for(auto [key, i] : materials.at(e.name)) {
				i->getParameter("u_diffuseColor")->setValue(e.color);
				i->getParameter("u_ambientColor")->setValue(e.ambientColor);

				std::string filePath(e.textureFilePath);
				if(!filePath.empty())
					i->getParameter("u_diffuseTexture")->setValue(filePath.c_str(), true);
				else
					i->getParameter("u_diffuseTexture")->setValue("resource/DefaultTexture.png", true);

				filePath = e.normalFilePath;
				if(!filePath.empty())
					i->getParameter("u_normalmapTexture")->setValue(filePath.c_str(), true);
				else
					i->getParameter("u_normalmapTexture")->setValue("resource/DefaultNormal.png", true);
			}
		}
	});

	EventDispatcher materialChanged(event);
	materialChanged.Dispatch<EventMaterialChanged>([&](EventMaterialChanged& e) {
		Material* material(nullptr);

		for(auto& [key, i] : materials)
			if(i.count(e.meshName)) {
				material = i.at(e.meshName);
				i.erase(e.meshName);
				break;
			}

		if(material) {
			material->getParameter("u_diffuseColor")->setValue(e.color);
			material->getParameter("u_ambientColor")->setValue(e.ambientColor);

			std::string filePath(e.textureFilePath);
			if(!filePath.empty())
				material->getParameter("u_diffuseTexture")->setValue(filePath.c_str(), true);
			else
				material->getParameter("u_diffuseTexture")->setValue("resource/DefaultTexture.png", true);

			filePath = e.normalFilePath;
			if(!filePath.empty())
				material->getParameter("u_normalmapTexture")->setValue(filePath.c_str(), true);
			else
				material->getParameter("u_normalmapTexture")->setValue("resource/DefaultNormal.png", true);

			if(!materials.count(e.newName))
				materials.emplace(e.newName, std::map<std::string, Material*>());

			materials.at(e.newName).emplace(e.meshName, material);
		}
	});

	EventDispatcher vertexModified(event);
	vertexModified.Dispatch<EventVertexModified>([&](EventVertexModified& e) {
		Node* node = _scene->findNode(e.name);

		if(node) {
			Drawable* drawable = node->getDrawable();
			Model* model = static_cast<Model*>(drawable);
			Mesh* mesh = model->getMesh();

			if(mesh->getVertexCount() == e.vertexCount) {
				Vertex* vertecies = NEW Vertex[e.vertexCount];
				memcpy(vertecies, &e + 1ull, sizeof(Vertex) * e.vertexCount);
				mesh->setVertexData(vertecies);
				delete[] vertecies;
			}
		}
	});

	EventDispatcher topologyModified(event);
	topologyModified.Dispatch<EventTopologyModified>([&](EventTopologyModified& e) {
		Node* node = _scene->findNode(e.name);

		if(node) {
			Drawable* drawable = node->getDrawable();
			Model* model = static_cast<Model*>(drawable);
			Mesh* mesh = model->getMesh();

			if(mesh->getVertexCount() != e.vertexCount) {
				Vertex* vertecies = NEW Vertex[e.vertexCount];
				memcpy(vertecies, &e + 1ull, sizeof(Vertex) * e.vertexCount);

				Mesh* newMesh = CreateMesh(vertecies, e.vertexCount);
				Model* newModel = Model::create(newMesh);
				newModel->setMaterial(model->getMaterial());
				node->setDrawable(newModel);

				delete[] vertecies;
			}
		}
	});

	char* memory = (char*)event;
	delete[] memory;
}

void MayaViewer::update(float elapsedTime) {

	bool done(false);
	while(!done) {
		Event* event(nullptr);
		done = !com.Inject((void**)&event);
		if(event) EventCallback(event);
	}

	for(auto& [key, i] : materials) {
		if(i.size() == 0ull) {
			materials.erase(key);
			break;
		}
	}
}

void MayaViewer::render(float elapsedTime) {
    clear(CLEAR_COLOR_DEPTH, Vector4(0.35f, 0.35f, 0.35f, 0.1f), 1.0f, 0);

    _scene->visit(this, &MayaViewer::drawScene);
}

bool MayaViewer::drawScene(Node* node) {
    Drawable* drawable = node->getDrawable(); 
    if (drawable)
        drawable->draw();

    return true;
}

void MayaViewer::keyEvent(Keyboard::KeyEvent evt, int key) {
    if (evt == Keyboard::KEY_PRESS) {
        switch (key) {
        case Keyboard::KEY_ESCAPE:
            exit();
            break;
        }
    }
}

void MayaViewer::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex) {
    switch (evt) {
    case Touch::TOUCH_PRESS:
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}
