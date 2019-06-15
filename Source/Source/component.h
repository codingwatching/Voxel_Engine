#pragma once
//#include "game_object.h"

typedef class GameObject* GameObjectPtr;

// defines all of the component types, and in which order they are updated
enum ComponentType : unsigned
{
	cTransform,
	cCollider,
	cLight,
	cButton,
	cMesh, // encapsulates vertices and texture information theoretically
	cDynamicPhysics,
	cKinematicPhysics,
	cText,
	cScripts,
	cRenderData,

	cCount
};

// augments game objects
typedef class Component
{
public:
	inline void SetType(ComponentType t) { _type = t; }
	inline void SetParent(GameObjectPtr p) { _parent = p; }
	inline void SetEnabled(bool e) { _enabled = e; }

	inline ComponentType GetType() const { return _type; }
	inline GameObjectPtr GetParent() const { return _parent; }
	inline bool GetEnabled() const { return _enabled; }

	virtual void Update(float dt) {}
	virtual ~Component() {}
	virtual Component* Clone() const { return nullptr; };

private:
	GameObjectPtr _parent;	// what object it's attached to
	ComponentType _type;		// what type of component
	bool _enabled = true;		// user var

}Component;