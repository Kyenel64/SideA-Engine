#include "Lpch.h"
#include "Scene.h"

#include <glm/glm.hpp>
#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>

#include "Locus/Renderer/Renderer2D.h"
#include "Locus/Renderer/RenderCommand.h"
#include "Locus/Renderer/EditorCamera.h"
#include "Locus/Scene/Components.h"
#include "Locus/Scene/ScriptableEntity.h"
#include "Locus/Scene/Entity.h"
#include "Locus/Scripting/ScriptEngine.h"
#include "Locus/Physics2D/ContactListener2D.h"
#include "Locus/Physics2D/PhysicsUtils.h"

namespace Locus
{
	Scene::Scene()
	{
		m_ContactListener = CreateRef<ContactListener2D>();
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other)
	{
		Ref<Scene> newScene = CreateRef<Scene>();
		newScene->m_SceneName = other->m_SceneName;
		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

		auto& otherRegistry = other->m_Registry;

		// Display each entity
		otherRegistry.each([&](auto entityID)
			{
				Entity entity = Entity(entityID, other.get());
				Entity newEntity = newScene->CreateEntityWithUUID(entity.GetUUID());
				CopyAllComponents(entity, newEntity);
			});

		// Convert all Entity references to newScene since copying 
		newScene->m_Registry.each([&](auto entityID)
			{
				Entity entity = Entity(entityID, newScene.get());
				auto& tc = entity.GetComponent<TransformComponent>();
				if (tc.Parent != Entity::Null)
					tc.Parent = newScene->GetEntityByUUID(tc.Parent.GetUUID());

				if (entity.HasComponent<ChildComponent>())
				{
					auto& cc = entity.GetComponent<ChildComponent>();
					for (uint32_t i = 0; i < cc.ChildCount; i++)
					{
						cc.ChildEntities[i] = newScene->GetEntityByUUID(cc.ChildEntities[i].GetUUID());
					}
				}
			});

		return newScene;
	}

	template<typename T>
	void Scene::CopyComponent(Entity from, Entity to)
	{
		if (from.HasComponent<T>())
			to.AddOrReplaceComponent<T>(from.GetComponent<T>());
	}

	void Scene::CopyAllComponents(Entity from, Entity to)
	{
		CopyComponent<TagComponent>(from, to);
		CopyComponent<ChildComponent>(from, to);
		CopyComponent<TransformComponent>(from, to);
		CopyComponent<SpriteRendererComponent>(from, to);
		CopyComponent<CircleRendererComponent>(from, to);
		CopyComponent<CameraComponent>(from, to);
		CopyComponent<Rigidbody2DComponent>(from, to);
		CopyComponent<BoxCollider2DComponent>(from, to);
		CopyComponent<CircleCollider2DComponent>(from, to);
		CopyComponent<NativeScriptComponent>(from, to);
		CopyComponent<ScriptComponent>(from, to);
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name, bool enabled)
	{
		Entity entity = Entity(m_Registry.create(), this);
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		entity.GetComponent<TransformComponent>().Self = entity;
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		tag.Group = "Default";
		tag.Enabled = enabled;
		tag.HierarchyPos = m_RootEntityCount;
		m_RootEntityCount++;
		return entity;
	}

	
	Entity Scene::CreateEntityWithUUID(Entity copyEntity, UUID uuid, const std::string& name, bool enabled)
	{
		Entity entity = Entity(m_Registry.create(copyEntity), this);
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		entity.GetComponent<TransformComponent>().Self = entity;
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Entity" : name;
		tag.Group = "Default";
		tag.Enabled = enabled;
		tag.HierarchyPos = m_RootEntityCount;
		m_RootEntityCount++;
		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		m_Registry.destroy(entity);
	}

	void Scene::OnUpdateRuntime(Timestep deltaTime)
	{
		// --- Physics ---
		// Check physics data for changes
		{
			auto view = m_Registry.view<IDComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				UpdatePhysicsData(entity);
			}
		}
		// Simulate physics
		{
			m_Box2DWorld->Step(deltaTime, 6, 2); // TODO: paremeterize
			m_ContactListener->Execute();
			auto view = m_Registry.view<Rigidbody2DComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				if (entity.GetComponent<TagComponent>().Enabled)
				{
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;
					const b2Vec2& position = body->GetPosition();
					transform.LocalPosition = { position.x, position.y , 0.0f };
					transform.SetLocalRotation({ 0, 0, body->GetAngle() });
				}
			}
		}
		// --- Update Native Scripts ---
		{
			auto view = m_Registry.view<NativeScriptComponent, TagComponent>();
			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				bool enabled = view.get<TagComponent>(entity).Enabled;
				if (enabled)
				{
					nsc.Instance->OnUpdate(deltaTime);
				}
			}
		}

		// --- Update C# Scripts ---
		{
			// Example API
			auto view = m_Registry.view<ScriptComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				if (view.get<TagComponent>(e).Enabled)
				{
					ScriptEngine::OnUpdateEntityScript(entity, deltaTime);
				}
			}
		}

		// --- Rendering 2D ---
		// Find first camera with "Primary" property enabled.
		SceneCamera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				auto& camera = entity.GetComponent<CameraComponent>();
				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					mainCamera->SetViewportSize(m_ViewportWidth, m_ViewportHeight);
					cameraTransform = GetWorldTransform(entity);
					break;
				}
			}
		}
		// Render if main camera exists
		if (mainCamera)
		{
			// Main rendering
			RenderCommand::SetClearColor(mainCamera->GetBackgroundColor());
			RenderCommand::Clear();

			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			{ // Sprite
				auto view = m_Registry.view<TransformComponent, SpriteRendererComponent, TagComponent>();
				for (auto e : view)
				{
					Entity entity = Entity(e, this);
					bool enabled = entity.GetComponent<TagComponent>().Enabled;
					auto& sprite = entity.GetComponent<SpriteRendererComponent>();
					if (enabled)
						Renderer2D::DrawSprite(GetWorldTransform(entity), sprite, (int)e);
				}
			}
			
			{ // Circle
				auto view = m_Registry.view<TransformComponent, CircleRendererComponent, TagComponent>();
				for (auto e : view)
				{
					Entity entity = Entity(e, this);
					bool enabled = entity.GetComponent<TagComponent>().Enabled;
					auto& circle = entity.GetComponent<CircleRendererComponent>();
					if (enabled)
						Renderer2D::DrawCircle(GetWorldTransform(entity), circle.Color, circle.Thickness, circle.Fade, (int)e);
				}
			}
			
			Renderer2D::EndScene();
		}
		else
		{
			RenderCommand::SetClearColor({ 0.2f, 0.2f, 0.25f, 1 });
			RenderCommand::Clear();
		}
	}

	void Scene::OnUpdateEditor(Timestep deltaTime, EditorCamera& camera)
	{
		// Main rendering
		Renderer2D::BeginScene(camera);

		{ // Sprite
			auto view = m_Registry.view<TransformComponent, SpriteRendererComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				bool enabled = entity.GetComponent<TagComponent>().Enabled;
				auto& sprite = entity.GetComponent<SpriteRendererComponent>();
				if (enabled)
					Renderer2D::DrawSprite(GetWorldTransform(entity), sprite, (int)e);
			}
		}

		{ // Circle
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				bool enabled = entity.GetComponent<TagComponent>().Enabled;
				auto& circle = entity.GetComponent<CircleRendererComponent>();
				if (enabled)
					Renderer2D::DrawCircle(GetWorldTransform(entity), circle.Color, circle.Thickness, circle.Fade, (int)e);
			}
		}

		// 3D grid plane
		if (camera.GetGridVisibility())
			Renderer2D::DrawGrid();

		Renderer2D::EndScene();
	}

	void Scene::OnUpdatePhysics(Timestep deltaTime, EditorCamera& camera)
	{
		// --- Physics ---
		// Check physics data for changes
		{
			auto view = m_Registry.view<IDComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				UpdatePhysicsData(entity);
			}
		}
		// Simulate physics
		{
			m_Box2DWorld->Step(deltaTime, 6, 2); // TODO: paremeterize
			auto view = m_Registry.view<Rigidbody2DComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				if (entity.GetComponent<TagComponent>().Enabled)
				{
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;
					const b2Vec2& position = body->GetPosition();
					transform.LocalPosition = { position.x, position.y , 0.0f };
					transform.SetLocalRotation({ 0, 0, body->GetAngle() });
				}
			}
		}

		// Main rendering
		Renderer2D::BeginScene(camera);

		// --- Sprite ---
		{
			auto view = m_Registry.view<TransformComponent, SpriteRendererComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				bool enabled = entity.GetComponent<TagComponent>().Enabled;
				auto& sprite = entity.GetComponent<SpriteRendererComponent>();
				if (enabled)
					Renderer2D::DrawSprite(GetWorldTransform(entity), sprite, (int)e);
			}
		}

		// --- Circle ---
		{
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent, TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				bool enabled = entity.GetComponent<TagComponent>().Enabled;
				auto& circle = entity.GetComponent<CircleRendererComponent>();
				if (enabled)
					Renderer2D::DrawCircle(GetWorldTransform(entity), circle.Color, circle.Thickness, circle.Fade, (int)e);
			}
		}

		// 3D grid plane
		if (camera.GetGridVisibility())
			Renderer2D::DrawGrid();

		Renderer2D::EndScene();
	}

	void Scene::OnRuntimeStart()
	{	
		// --- Physics ---
		{
			m_Box2DWorld = new b2World({ 0.0f, -9.8f });
			m_Box2DWorld->SetContactListener(m_ContactListener.get());

			auto view = m_Registry.view<TagComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				if (entity.GetComponent<TagComponent>().Enabled)
					CreatePhysicsData(entity);
			}
		}

		// --- Native Script ---
		{
			auto view = m_Registry.view<NativeScriptComponent, TagComponent>();
			for (auto entity : view)
			{
				auto& nsc = view.get<NativeScriptComponent>(entity);
				bool enabled = view.get<TagComponent>(entity).Enabled;
				if (enabled)
				{
					nsc.Instance = nsc.InstantiateScript();
					nsc.Instance->m_Entity = Entity(entity, this);
					nsc.Instance->OnCreate();
				}
			}
		}

		// --- C# Scripts ---
		{
			ScriptEngine::OnRuntimeStart(this);
			auto view = m_Registry.view<ScriptComponent, TagComponent, IDComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				auto& sc = view.get<ScriptComponent>(e);
				bool enabled = view.get<TagComponent>(e).Enabled;
				UUID id = view.get<IDComponent>(e).ID;
				if (enabled)
				{
					ScriptEngine::OnCreateEntityScript(entity);
				}
			}
		}
	}

	void Scene::OnPhysicsStart()
	{
		// --- Physics ---
		{
			m_Box2DWorld = new b2World({ 0.0f, -9.8f });

			auto view = m_Registry.view<IDComponent>();
			for (auto e : view)
			{
				Entity entity = Entity(e, this);
				if (entity.GetComponent<TagComponent>().Enabled)
					CreatePhysicsData(entity);
			}
		}
	}

	void Scene::OnRuntimeStop()
	{
		delete m_Box2DWorld;
		m_Box2DWorld = nullptr;

		ScriptEngine::OnRuntimeStop();
	}

	void Scene::CreatePhysicsData(Entity entity)
	{
		auto& tc = entity.GetComponent<TransformComponent>();

		b2Body* entityBody = nullptr;
		float mass = -1.0f;

		// If entity has a collider and rigidbody
		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2D = entity.GetComponent<Rigidbody2DComponent>();
			mass = rb2D.Mass;

			// Body
			b2BodyDef bodyDef;
			bodyDef.type = Utils::Rigidbody2DTypeToBox2DType(rb2D.BodyType);
			bodyDef.position.Set(tc.LocalPosition.x, tc.LocalPosition.y);
			bodyDef.angle = tc.LocalRotation.z;
			bodyDef.linearDamping = rb2D.LinearDamping;
			bodyDef.angularDamping = rb2D.AngularDamping;
			bodyDef.fixedRotation = rb2D.FixedRotation;
			bodyDef.gravityScale = rb2D.GravityScale;
			bodyDef.userData.pointer = (uintptr_t)entity.GetUUID();
			entityBody = m_Box2DWorld->CreateBody(&bodyDef);
			rb2D.RuntimeBody = entityBody;
			b2MassData massData;
			massData.mass = rb2D.Mass;
			massData.I = entityBody->GetInertia();
			massData.center = entityBody->GetLocalCenter();
			entityBody->SetMassData(&massData);
		}
		else
		{
			// If entity has a collider but no rigidbody
			if (entity.HasComponent<BoxCollider2DComponent>() || entity.HasComponent<CircleCollider2DComponent>())
			{
				mass = 0.0f;

				b2BodyDef bodyDef;
				bodyDef.type = b2BodyType::b2_staticBody;
				bodyDef.position.Set(tc.LocalPosition.x, tc.LocalPosition.y);
				bodyDef.angle = tc.LocalRotation.z;
				bodyDef.userData.pointer = (uintptr_t)entity.GetUUID();
				entityBody = m_Box2DWorld->CreateBody(&bodyDef);
			}
		}

		if (entity.HasComponent<BoxCollider2DComponent>()) // TODO: Check memory leak
		{
			auto& b2D = entity.GetComponent<BoxCollider2DComponent>();
			
			b2Vec2 size = { b2D.Size.x * tc.LocalScale.x, b2D.Size.y * tc.LocalScale.y };
			b2Vec2 offset = { tc.LocalScale.x * b2D.Offset.x, tc.LocalScale.y * b2D.Offset.y };
			float angle = 0.0f; // TODO

			b2PolygonShape box;
			box.SetAsBox(size.x / 2, size.y / 2, offset, angle);
			b2FixtureDef fixtureDef; // Move this to collider
			fixtureDef.density = mass; // TODO: Check if this is a correct way to handle density
			fixtureDef.friction = b2D.Friction;
			fixtureDef.restitution = b2D.Restitution;
			fixtureDef.restitutionThreshold = b2D.RestitutionThreshold;
			fixtureDef.shape = &box;
			fixtureDef.filter.categoryBits = b2D.CollisionCategory;
			fixtureDef.filter.maskBits = b2D.CollisionMask;
			b2Fixture* fixture = entityBody->CreateFixture(&fixtureDef);
			b2D.RuntimeFixture = fixture;
		}
		else if (entity.HasComponent<CircleCollider2DComponent>())
		{
			auto& c2D = entity.GetComponent<CircleCollider2DComponent>();

			float maxScale = tc.LocalScale.x > tc.LocalScale.y ? tc.LocalScale.x : tc.LocalScale.y;
			b2Vec2 offset = { maxScale * c2D.Offset.x, maxScale * c2D.Offset.y };
			float radius = maxScale * c2D.Radius;

			b2CircleShape circle;
			b2FixtureDef fixtureDef; // Move this to collider
			fixtureDef.density = mass;
			fixtureDef.friction = c2D.Friction;
			fixtureDef.restitution = c2D.Restitution;
			fixtureDef.restitutionThreshold = c2D.RestitutionThreshold;
			circle.m_p = offset;
			circle.m_radius = radius;
			fixtureDef.shape = &circle;
			fixtureDef.filter.categoryBits = c2D.CollisionCategory;
			fixtureDef.filter.maskBits = c2D.CollisionMask;
			b2Fixture* fixture = entityBody->CreateFixture(&fixtureDef);
			c2D.RuntimeFixture = fixture;
		}
	}

	void Scene::UpdatePhysicsData(Entity entity)
	{
		auto& tc = entity.GetComponent<TransformComponent>();

		// Create data if physics data doesn't exist
		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			if (entity.GetComponent<Rigidbody2DComponent>().RuntimeBody == nullptr)
				CreatePhysicsData(entity);
		}
		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			if (entity.GetComponent<BoxCollider2DComponent>().RuntimeFixture == nullptr)
				CreatePhysicsData(entity);
		}
		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			if (entity.GetComponent<CircleCollider2DComponent>().RuntimeFixture == nullptr)
				CreatePhysicsData(entity);
		}

		// Profile performance vs not checking each property for a change.
		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			auto& rb2D = entity.GetComponent<Rigidbody2DComponent>();
			b2Body* runtimeBody = (b2Body*)rb2D.RuntimeBody;
			
			if (runtimeBody->GetType() != Utils::Rigidbody2DTypeToBox2DType(rb2D.BodyType))
				runtimeBody->SetType(Utils::Rigidbody2DTypeToBox2DType(rb2D.BodyType));

			if (runtimeBody->GetMass() != rb2D.Mass)
			{
				b2MassData massData;
				massData.mass = rb2D.Mass;
				massData.I = runtimeBody->GetInertia();
				massData.center = runtimeBody->GetLocalCenter();
				runtimeBody->SetMassData(&massData);
			}

			if (runtimeBody->GetGravityScale() != rb2D.GravityScale)
				runtimeBody->SetGravityScale(rb2D.GravityScale);
			if (runtimeBody->GetLinearDamping() != rb2D.LinearDamping)
				runtimeBody->SetLinearDamping(rb2D.LinearDamping);
			if (runtimeBody->GetAngularDamping() != rb2D.AngularDamping)
				runtimeBody->SetAngularDamping(rb2D.AngularDamping);
			if (runtimeBody->IsFixedRotation() != rb2D.FixedRotation)
				runtimeBody->SetFixedRotation(rb2D.FixedRotation);
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			auto& b2D = entity.GetComponent<BoxCollider2DComponent>();

			b2Vec2 size = { b2D.Size.x * tc.LocalScale.x, b2D.Size.y * tc.LocalScale.y };
			b2Vec2 offset = { tc.LocalScale.x * b2D.Offset.x, tc.LocalScale.y * b2D.Offset.y };
			float angle = 0.0f; // TODO
			float mass = entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().Mass : 0.0f;

			b2Fixture* runtimeFixture = (b2Fixture*)b2D.RuntimeFixture;
			b2PolygonShape* shape = (b2PolygonShape*)runtimeFixture->GetShape();

			// TODO: Collision jittery when changed during runtime
			shape->SetAsBox(size.x / 2, size.y / 2, offset, angle);
			// Check if shape is changed
			/*b2PolygonShape thisShape;
			thisShape.SetAsBox(size.x / 2, size.y / 2, offset, angle);
			if (thisShape.m_vertices[0] != shape->m_vertices[0] || thisShape.m_vertices[1] != shape->m_vertices[1]
				|| thisShape.m_vertices[2] != shape->m_vertices[2] || thisShape.m_vertices[3] != shape->m_vertices[3])
			{
				for (int i = 0; i < 4; i++)
					shape->m_vertices[i] = thisShape.m_vertices[i];
			}*/
			if (runtimeFixture->GetDensity() != mass)
				runtimeFixture->SetDensity(mass);
			if (runtimeFixture->GetFriction() != b2D.Friction)
				runtimeFixture->SetFriction(b2D.Friction);
			if (runtimeFixture->GetRestitution() != b2D.Restitution)
				runtimeFixture->SetRestitution(b2D.Restitution);
			if (runtimeFixture->GetRestitutionThreshold() != b2D.RestitutionThreshold)
				runtimeFixture->SetRestitutionThreshold(b2D.RestitutionThreshold);
			if (runtimeFixture->GetFilterData().categoryBits != b2D.CollisionCategory || runtimeFixture->GetFilterData().maskBits != b2D.CollisionMask)
			{
				b2Filter filter;
				filter.categoryBits = b2D.CollisionCategory;
				filter.maskBits = b2D.CollisionMask;
				runtimeFixture->SetFilterData(filter);
			}
		}

		if (entity.HasComponent<CircleRendererComponent>())
		{
			auto& c2D = entity.GetComponent<CircleCollider2DComponent>();

			float maxScale = tc.LocalScale.x > tc.LocalScale.y ? tc.LocalScale.x : tc.LocalScale.y;
			b2Vec2 offset = { maxScale * c2D.Offset.x, maxScale * c2D.Offset.y };
			float radius = maxScale * c2D.Radius;
			float mass = entity.HasComponent<Rigidbody2DComponent>() ? entity.GetComponent<Rigidbody2DComponent>().Mass : 0.0f;

			b2Fixture* runtimeFixture = (b2Fixture*)c2D.RuntimeFixture;
			b2CircleShape* shape = (b2CircleShape*)runtimeFixture->GetShape();
			
			if (shape->m_radius != radius)
				shape->m_radius = radius;
			if (shape->m_p != offset)
				shape->m_p = offset;
			if (runtimeFixture->GetDensity() != mass)
				runtimeFixture->SetDensity(mass);
			if (runtimeFixture->GetFriction() != c2D.Friction)
				runtimeFixture->SetFriction(c2D.Friction);
			if (runtimeFixture->GetRestitution() != c2D.Restitution)
				runtimeFixture->SetRestitution(c2D.Restitution);
			if (runtimeFixture->GetRestitutionThreshold() != c2D.RestitutionThreshold)
				runtimeFixture->SetRestitutionThreshold(c2D.RestitutionThreshold);
			if (runtimeFixture->GetFilterData().categoryBits != c2D.CollisionCategory || runtimeFixture->GetFilterData().maskBits != c2D.CollisionMask)
			{
				b2Filter filter;
				filter.categoryBits = c2D.CollisionCategory;
				filter.maskBits = c2D.CollisionMask;
				runtimeFixture->SetFilterData(filter);
			}
		}
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			if (view.get<CameraComponent>(entity).Primary)
				return Entity(entity, this);
		}
		return Entity::Null;
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto view = m_Registry.view<IDComponent>();
		for (auto e : view)
		{
			Entity entity = Entity(e, this);
			if (entity.GetComponent<IDComponent>().ID == uuid)
				return entity;
		}

		return Entity::Null;
	}

	glm::mat4 Scene::GetWorldTransform(Entity entity)
	{
		glm::mat4 transform(1.0f);
		auto& tc = entity.GetComponent<TransformComponent>();

		if (tc.Parent != Entity::Null)
			transform = GetWorldTransform(tc.Parent);

		return transform * tc.GetLocalTransform();
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		static_assert<false>;
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<ChildComponent>(Entity entity, ChildComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component)
	{

	}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{

	}
}
