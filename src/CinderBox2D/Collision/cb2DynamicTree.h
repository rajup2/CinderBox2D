/*
* Copyright (c) 2009 Erin Catto http://www.box2d.org
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

#ifndef CB2_DYNAMIC_TREE_H
#define CB2_DYNAMIC_TREE_H

#include <CinderBox2D/Collision/cb2Collision.h>
#include <CinderBox2D/Common/cb2GrowableStack.h>

#define cb2_nullNode (-1)

/// A node in the dynamic tree. The client does not interact with this directly.
struct cb2TreeNode
{
	bool IsLeaf() const
	{
		return child1 == cb2_nullNode;
	}

	/// Enlarged AABB
	cb2AABB aabb;

	void* userData;

	union
	{
		int parent;
		int next;
	};

	int child1;
	int child2;

	// leaf = 0, free node = -1
	int height;
};

/// A dynamic AABB tree broad-phase, inspired by Nathanael Presson's btDbvt.
/// A dynamic tree arranges data in a binary tree to accelerate
/// queries such as volume queries and ray casts. Leafs are proxies
/// with an AABB. In the tree we expand the proxy AABB by cb2_fatAABBFactor
/// so that the proxy AABB is bigger than the client object. This allows the client
/// object to move by small amounts without triggering a tree update.
///
/// Nodes are pooled and relocatable, so we use node indices rather than pointers.
class cb2DynamicTree
{
public:
	/// Constructing the tree initializes the node pool.
	cb2DynamicTree();

	/// Destroy the tree, freeing the node pool.
	~cb2DynamicTree();

	/// Create a proxy. Provide a tight fitting AABB and a userData pointer.
	int CreateProxy(const cb2AABB& aabb, void* userData);

	/// Destroy a proxy. This asserts if the id is invalid.
	void DestroyProxy(int proxyId);

	/// Move a proxy with a swepted AABB. If the proxy has moved outside of its fattened AABB,
	/// then the proxy is removed from the tree and re-inserted. Otherwise
	/// the function returns immediately.
	/// @return true if the proxy was re-inserted.
	bool MoveProxy(int proxyId, const cb2AABB& aabb1, const ci::Vec2f& displacement);

	/// Get proxy user data.
	/// @return the proxy user data or 0 if the id is invalid.
	void* GetUserData(int proxyId) const;

	/// Get the fat AABB for a proxy.
	const cb2AABB& GetFatAABB(int proxyId) const;

	/// Query an AABB for overlapping proxies. The callback class
	/// is called for each proxy that overlaps the supplied AABB.
	template <typename T>
	void Query(T* callback, const cb2AABB& aabb) const;

	/// Ray-cast against the proxies in the tree. This relies on the callback
	/// to perform a exact ray-cast in the case were the proxy contains a shape.
	/// The callback also performs the any collision filtering. This has performance
	/// roughly equal to k * log(n), where k is the number of collisions and n is the
	/// number of proxies in the tree.
	/// @param input the ray-cast input data. The ray extends from p1 to p1 + maxFraction * (p2 - p1).
	/// @param callback a callback class that is called for each proxy that is hit by the ray.
	template <typename T>
	void RayCast(T* callback, const cb2RayCastInput& input) const;

	/// Validate this tree. For testing.
	void Validate() const;

	/// Compute the height of the binary tree in O(N) time. Should not be
	/// called often.
	int GetHeight() const;

	/// Get the maximum balance of an node in the tree. The balance is the difference
	/// in height of the two children of a node.
	int GetMaxBalance() const;

	/// Get the ratio of the sum of the node areas to the root area.
	float GetAreaRatio() const;

	/// Build an optimal tree. Very expensive. For testing.
	void RebuildBottomUp();

	/// Shift the world origin. Useful for large worlds.
	/// The shift formula is: position -= newOrigin
	/// @param newOrigin the new origin with respect to the old origin
	void ShiftOrigin(const ci::Vec2f& newOrigin);

private:

	int AllocateNode();
	void FreeNode(int node);

	void InsertLeaf(int node);
	void RemoveLeaf(int node);

	int Balance(int index);

	int ComputeHeight() const;
	int ComputeHeight(int nodeId) const;

	void ValidateStructure(int index) const;
	void ValidateMetrics(int index) const;

	int m_root;

	cb2TreeNode* m_nodes;
	int m_nodeCount;
	int m_nodeCapacity;

	int m_freeList;

	/// This is used to incrementally traverse the tree for re-balancing.
	unsigned int m_path;

	int m_insertionCount;
};

inline void* cb2DynamicTree::GetUserData(int proxyId) const
{
	cb2Assert(0 <= proxyId && proxyId < m_nodeCapacity);
	return m_nodes[proxyId].userData;
}

inline const cb2AABB& cb2DynamicTree::GetFatAABB(int proxyId) const
{
	cb2Assert(0 <= proxyId && proxyId < m_nodeCapacity);
	return m_nodes[proxyId].aabb;
}

template <typename T>
inline void cb2DynamicTree::Query(T* callback, const cb2AABB& aabb) const
{
	cb2GrowableStack<int, 256> stack;
	stack.Push(m_root);

	while (stack.GetCount() > 0)
	{
		int nodeId = stack.Pop();
		if (nodeId == cb2_nullNode)
		{
			continue;
		}

		const cb2TreeNode* node = m_nodes + nodeId;

		if (cb2TestOverlap(node->aabb, aabb))
		{
			if (node->IsLeaf())
			{
				bool proceed = callback->QueryCallback(nodeId);
				if (proceed == false)
				{
					return;
				}
			}
			else
			{
				stack.Push(node->child1);
				stack.Push(node->child2);
			}
		}
	}
}

template <typename T>
inline void cb2DynamicTree::RayCast(T* callback, const cb2RayCastInput& input) const
{
	ci::Vec2f p1 = input.p1;
	ci::Vec2f p2 = input.p2;
	ci::Vec2f r = p2 - p1;
	cb2Assert(r.lengthSquared() > 0.0f);
	r.normalize();

	// v is perpendicular to the segment.
	ci::Vec2f v = cb2Cross(1.0f, r);
	ci::Vec2f abs_v = cb2Abs(v);

	// Separating axis for segment (Gino, p80).
	// |dot(v, p1 - c)| > dot(|v|, h)

	float maxFraction = input.maxFraction;

	// Build a bounding box for the segment.
	cb2AABB segmentAABB;
	{
		ci::Vec2f t = p1 + maxFraction * (p2 - p1);
		segmentAABB.lowerBound = cb2Min(p1, t);
		segmentAABB.upperBound = cb2Max(p1, t);
	}

	cb2GrowableStack<int, 256> stack;
	stack.Push(m_root);

	while (stack.GetCount() > 0)
	{
		int nodeId = stack.Pop();
		if (nodeId == cb2_nullNode)
		{
			continue;
		}

		const cb2TreeNode* node = m_nodes + nodeId;

		if (cb2TestOverlap(node->aabb, segmentAABB) == false)
		{
			continue;
		}

		// Separating axis for segment (Gino, p80).
		// |dot(v, p1 - c)| > dot(|v|, h)
		ci::Vec2f c = node->aabb.GetCenter();
		ci::Vec2f h = node->aabb.GetExtents();
		float separation = cb2Abs(cb2Dot(v, p1 - c)) - cb2Dot(abs_v, h);
		if (separation > 0.0f)
		{
			continue;
		}

		if (node->IsLeaf())
		{
			cb2RayCastInput subInput;
			subInput.p1 = input.p1;
			subInput.p2 = input.p2;
			subInput.maxFraction = maxFraction;

			float value = callback->RayCastCallback(subInput, nodeId);

			if (value == 0.0f)
			{
				// The client has terminated the ray cast.
				return;
			}

			if (value > 0.0f)
			{
				// Update segment bounding box.
				maxFraction = value;
				ci::Vec2f t = p1 + maxFraction * (p2 - p1);
				segmentAABB.lowerBound = cb2Min(p1, t);
				segmentAABB.upperBound = cb2Max(p1, t);
			}
		}
		else
		{
			stack.Push(node->child1);
			stack.Push(node->child2);
		}
	}
}

#endif
