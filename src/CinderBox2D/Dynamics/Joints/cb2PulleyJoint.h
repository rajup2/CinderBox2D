/*
* Copyright (c) 2006-2011 Erin Catto http://www.box2d.org
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

#ifndef CB2_PULLEY_JOINT_H
#define CB2_PULLEY_JOINT_H

#include <CinderBox2D/Dynamics/Joints/cb2Joint.h>

const float cb2_minPulleyLength = 2.0f;

/// Pulley joint definition. This requires two ground anchors,
/// two dynamic body anchor points, and a pulley ratio.
struct cb2PulleyJointDef : public cb2JointDef
{
	cb2PulleyJointDef()
	{
		type = e_pulleyJoint;
		groundAnchorA.set(-1.0f, 1.0f);
		groundAnchorB.set(1.0f, 1.0f);
		localAnchorA.set(-1.0f, 0.0f);
		localAnchorB.set(1.0f, 0.0f);
		lengthA = 0.0f;
		lengthB = 0.0f;
		ratio = 1.0f;
		collideConnected = true;
	}

	/// Initialize the bodies, anchors, lengths, max lengths, and ratio using the world anchors.
	void Initialize(cb2Body* bodyA, cb2Body* bodyB,
					const ci::Vec2f& groundAnchorA, const ci::Vec2f& groundAnchorB,
					const ci::Vec2f& anchorA, const ci::Vec2f& anchorB,
					float ratio);

	/// The first ground anchor in world coordinates. This point never moves.
	ci::Vec2f groundAnchorA;

	/// The second ground anchor in world coordinates. This point never moves.
	ci::Vec2f groundAnchorB;

	/// The local anchor point relative to bodyA's origin.
	ci::Vec2f localAnchorA;

	/// The local anchor point relative to bodyB's origin.
	ci::Vec2f localAnchorB;

	/// The a reference length for the segment attached to bodyA.
	float lengthA;

	/// The a reference length for the segment attached to bodyB.
	float lengthB;

	/// The pulley ratio, used to simulate a block-and-tackle.
	float ratio;
};

/// The pulley joint is connected to two bodies and two fixed ground points.
/// The pulley supports a ratio such that:
/// length1 + ratio * length2 <= constant
/// Yes, the force transmitted is scaled by the ratio.
/// Warning: the pulley joint can get a bit squirrelly by itself. They often
/// work better when combined with prismatic joints. You should also cover the
/// the anchor points with static shapes to prevent one side from going to
/// zero length.
class cb2PulleyJoint : public cb2Joint
{
public:
	ci::Vec2f GetAnchorA() const;
	ci::Vec2f GetAnchorB() const;

	ci::Vec2f GetReactionForce(float inv_dt) const;
	float GetReactionTorque(float inv_dt) const;

	/// Get the first ground anchor.
	ci::Vec2f GetGroundAnchorA() const;

	/// Get the second ground anchor.
	ci::Vec2f GetGroundAnchorB() const;

	/// Get the current length of the segment attached to bodyA.
	float GetLengthA() const;

	/// Get the current length of the segment attached to bodyB.
	float GetLengthB() const;

	/// Get the pulley ratio.
	float GetRatio() const;

	/// Get the current length of the segment attached to bodyA.
	float GetCurrentLengthA() const;

	/// Get the current length of the segment attached to bodyB.
	float GetCurrentLengthB() const;

	/// Dump joint to dmLog
	void Dump();

	/// Implement cb2Joint::ShiftOrigin
	void ShiftOrigin(const ci::Vec2f& newOrigin);

protected:

	friend class cb2Joint;
	cb2PulleyJoint(const cb2PulleyJointDef* data);

	void InitVelocityConstraints(const cb2SolverData& data);
	void SolveVelocityConstraints(const cb2SolverData& data);
	bool SolvePositionConstraints(const cb2SolverData& data);

	ci::Vec2f m_groundAnchorA;
	ci::Vec2f m_groundAnchorB;
	float m_lengthA;
	float m_lengthB;
	
	// Solver shared
	ci::Vec2f m_localAnchorA;
	ci::Vec2f m_localAnchorB;
	float m_constant;
	float m_ratio;
	float m_impulse;

	// Solver temp
	int m_indexA;
	int m_indexB;
	ci::Vec2f m_uA;
	ci::Vec2f m_uB;
	ci::Vec2f m_rA;
	ci::Vec2f m_rB;
	ci::Vec2f m_localCenterA;
	ci::Vec2f m_localCenterB;
	float m_invMassA;
	float m_invMassB;
	float m_invIA;
	float m_invIB;
	float m_mass;
};

#endif
