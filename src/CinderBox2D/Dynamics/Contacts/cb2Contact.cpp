/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#include <CinderBox2D/Dynamics/Contacts/cb2Contact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2CircleContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2PolygonAndCircleContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2PolygonContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2EdgeAndCircleContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2EdgeAndPolygonContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2ChainAndCircleContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2ChainAndPolygonContact.h>
#include <CinderBox2D/Dynamics/Contacts/cb2ContactSolver.h>

#include <CinderBox2D/Collision/cb2Collision.h>
#include <CinderBox2D/Collision/cb2TimeOfImpact.h>
#include <CinderBox2D/Collision/Shapes/cb2Shape.h>
#include <CinderBox2D/Common/cb2BlockAllocator.h>
#include <CinderBox2D/Dynamics/cb2Body.h>
#include <CinderBox2D/Dynamics/cb2Fixture.h>
#include <CinderBox2D/Dynamics/cb2World.h>

cb2ContactRegister cb2Contact::s_registers[cb2Shape::e_typeCount][cb2Shape::e_typeCount];
bool cb2Contact::s_initialized = false;

void cb2Contact::InitializeRegisters()
{
	AddType(cb2CircleContact::Create, cb2CircleContact::Destroy, cb2Shape::e_circle, cb2Shape::e_circle);
	AddType(cb2PolygonAndCircleContact::Create, cb2PolygonAndCircleContact::Destroy, cb2Shape::e_polygon, cb2Shape::e_circle);
	AddType(cb2PolygonContact::Create, cb2PolygonContact::Destroy, cb2Shape::e_polygon, cb2Shape::e_polygon);
	AddType(cb2EdgeAndCircleContact::Create, cb2EdgeAndCircleContact::Destroy, cb2Shape::e_edge, cb2Shape::e_circle);
	AddType(cb2EdgeAndPolygonContact::Create, cb2EdgeAndPolygonContact::Destroy, cb2Shape::e_edge, cb2Shape::e_polygon);
	AddType(cb2ChainAndCircleContact::Create, cb2ChainAndCircleContact::Destroy, cb2Shape::e_chain, cb2Shape::e_circle);
	AddType(cb2ChainAndPolygonContact::Create, cb2ChainAndPolygonContact::Destroy, cb2Shape::e_chain, cb2Shape::e_polygon);
}

void cb2Contact::AddType(cb2ContactCreateFcn* createFcn, cb2ContactDestroyFcn* destoryFcn,
						cb2Shape::Type type1, cb2Shape::Type type2)
{
	cb2Assert(0 <= type1 && type1 < cb2Shape::e_typeCount);
	cb2Assert(0 <= type2 && type2 < cb2Shape::e_typeCount);
	
	s_registers[type1][type2].createFcn = createFcn;
	s_registers[type1][type2].destroyFcn = destoryFcn;
	s_registers[type1][type2].primary = true;

	if (type1 != type2)
	{
		s_registers[type2][type1].createFcn = createFcn;
		s_registers[type2][type1].destroyFcn = destoryFcn;
		s_registers[type2][type1].primary = false;
	}
}

cb2Contact* cb2Contact::Create(cb2Fixture* fixtureA, int indexA, cb2Fixture* fixtureB, int indexB, cb2BlockAllocator* allocator)
{
	if (s_initialized == false)
	{
		InitializeRegisters();
		s_initialized = true;
	}

	cb2Shape::Type type1 = fixtureA->GetType();
	cb2Shape::Type type2 = fixtureB->GetType();

	cb2Assert(0 <= type1 && type1 < cb2Shape::e_typeCount);
	cb2Assert(0 <= type2 && type2 < cb2Shape::e_typeCount);
	
	cb2ContactCreateFcn* createFcn = s_registers[type1][type2].createFcn;
	if (createFcn)
	{
		if (s_registers[type1][type2].primary)
		{
			return createFcn(fixtureA, indexA, fixtureB, indexB, allocator);
		}
		else
		{
			return createFcn(fixtureB, indexB, fixtureA, indexA, allocator);
		}
	}
	else
	{
		return NULL;
	}
}

void cb2Contact::Destroy(cb2Contact* contact, cb2BlockAllocator* allocator)
{
	cb2Assert(s_initialized == true);

	cb2Fixture* fixtureA = contact->m_fixtureA;
	cb2Fixture* fixtureB = contact->m_fixtureB;

	if (contact->m_manifold.pointCount > 0 &&
		fixtureA->IsSensor() == false &&
		fixtureB->IsSensor() == false)
	{
		fixtureA->GetBody()->SetAwake(true);
		fixtureB->GetBody()->SetAwake(true);
	}

	cb2Shape::Type typeA = fixtureA->GetType();
	cb2Shape::Type typeB = fixtureB->GetType();

	cb2Assert(0 <= typeA && typeB < cb2Shape::e_typeCount);
	cb2Assert(0 <= typeA && typeB < cb2Shape::e_typeCount);

	cb2ContactDestroyFcn* destroyFcn = s_registers[typeA][typeB].destroyFcn;
	destroyFcn(contact, allocator);
}

cb2Contact::cb2Contact(cb2Fixture* fA, int indexA, cb2Fixture* fB, int indexB)
{
	m_flags = e_enabledFlag;

	m_fixtureA = fA;
	m_fixtureB = fB;

	m_indexA = indexA;
	m_indexB = indexB;

	m_manifold.pointCount = 0;

	m_prev = NULL;
	m_next = NULL;

	m_nodeA.contact = NULL;
	m_nodeA.prev = NULL;
	m_nodeA.next = NULL;
	m_nodeA.other = NULL;

	m_nodeB.contact = NULL;
	m_nodeB.prev = NULL;
	m_nodeB.next = NULL;
	m_nodeB.other = NULL;

	m_toiCount = 0;

	m_friction = cb2MixFriction(m_fixtureA->m_friction, m_fixtureB->m_friction);
	m_restitution = cb2MixRestitution(m_fixtureA->m_restitution, m_fixtureB->m_restitution);

	m_tangentSpeed = 0.0f;
}

// Update the contact manifold and touching status.
// Note: do not assume the fixture AABBs are overlapping or are valid.
void cb2Contact::Update(cb2ContactListener* listener)
{
	cb2Manifold oldManifold = m_manifold;

	// Re-enable this contact.
	m_flags |= e_enabledFlag;

	bool touching = false;
	bool wasTouching = (m_flags & e_touchingFlag) == e_touchingFlag;

	bool sensorA = m_fixtureA->IsSensor();
	bool sensorB = m_fixtureB->IsSensor();
	bool sensor = sensorA || sensorB;

	cb2Body* bodyA = m_fixtureA->GetBody();
	cb2Body* bodyB = m_fixtureB->GetBody();
	const cb2Transform& xfA = bodyA->GetTransform();
	const cb2Transform& xfB = bodyB->GetTransform();

	// Is this contact a sensor?
	if (sensor)
	{
		const cb2Shape* shapeA = m_fixtureA->GetShape();
		const cb2Shape* shapeB = m_fixtureB->GetShape();
		touching = cb2TestOverlap(shapeA, m_indexA, shapeB, m_indexB, xfA, xfB);

		// Sensors don't generate manifolds.
		m_manifold.pointCount = 0;
	}
	else
	{
		Evaluate(&m_manifold, xfA, xfB);
		touching = m_manifold.pointCount > 0;

		// Match old contact ids to new contact ids and copy the
		// stored impulses to warm start the solver.
		for (int i = 0; i < m_manifold.pointCount; ++i)
		{
			cb2ManifoldPoint* mp2 = m_manifold.points + i;
			mp2->normalImpulse = 0.0f;
			mp2->tangentImpulse = 0.0f;
			cb2ContactID id2 = mp2->id;

			for (int j = 0; j < oldManifold.pointCount; ++j)
			{
				cb2ManifoldPoint* mp1 = oldManifold.points + j;

				if (mp1->id.key == id2.key)
				{
					mp2->normalImpulse = mp1->normalImpulse;
					mp2->tangentImpulse = mp1->tangentImpulse;
					break;
				}
			}
		}

		if (touching != wasTouching)
		{
			bodyA->SetAwake(true);
			bodyB->SetAwake(true);
		}
	}

	if (touching)
	{
		m_flags |= e_touchingFlag;
	}
	else
	{
		m_flags &= ~e_touchingFlag;
	}

	if (wasTouching == false && touching == true && listener)
	{
		listener->BeginContact(this);
	}

	if (wasTouching == true && touching == false && listener)
	{
		listener->EndContact(this);
	}

	if (sensor == false && touching && listener)
	{
		listener->PreSolve(this, &oldManifold);
	}
}
