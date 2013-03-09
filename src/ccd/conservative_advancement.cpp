/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */

#include "fcl/ccd/conservative_advancement.h"
#include "fcl/ccd/motion.h"
#include "fcl/collision_node.h"
#include "fcl/traversal/traversal_node_bvhs.h"
#include "fcl/traversal/traversal_node_setup.h"
#include "fcl/traversal/traversal_recurse.h"
#include "fcl/collision.h"


namespace fcl

{

template<typename BV, typename ConservativeAdvancementNode, typename CollisionNode>
int conservativeAdvancement(const CollisionGeometry* o1,
                            const MotionBase* motion1,
                            const CollisionGeometry* o2,
                            const MotionBase* motion2,
                            const CollisionRequest& request,
                            CollisionResult& result,
                            FCL_REAL& toc)
{
  if(request.num_max_contacts == 0)
  {
    std::cerr << "Warning: should stop early as num_max_contact is " << request.num_max_contacts << " !" << std::endl;
    return 0;
  }

  const OBJECT_TYPE object_type1 = o1->getObjectType();
  const NODE_TYPE node_type1 = o1->getNodeType();

  const OBJECT_TYPE object_type2 = o2->getObjectType();
  const NODE_TYPE node_type2 = o2->getNodeType();

  if(object_type1 != OT_BVH || object_type2 != OT_BVH)
    return 0;

  if(node_type1 != BV_RSS || node_type2 != BV_RSS)
    return 0;


  const BVHModel<BV>* model1 = static_cast<const BVHModel<BV>*>(o1);
  const BVHModel<BV>* model2 = static_cast<const BVHModel<BV>*>(o2);

  Transform3f tf1, tf2;
  motion1->getCurrentTransform(tf1);
  motion2->getCurrentTransform(tf2);

  // whether the first start configuration is in collision
  CollisionNode cnode;
  if(!initialize(cnode, *model1, tf1, *model2, tf2, request, result))
    std::cout << "initialize error" << std::endl;

  relativeTransform(tf1.getRotation(), tf1.getTranslation(), tf2.getRotation(), tf2.getTranslation(), cnode.R, cnode.T);

  cnode.enable_statistics = false;
  cnode.request = request;

  collide(&cnode);

  int b = result.numContacts();

  if(b > 0)
  {
    toc = 0;
    // std::cout << "zero collide" << std::endl;
    return b;
  }
  
  ConservativeAdvancementNode node;

  initialize(node, *model1, tf1, *model2, tf2);

  node.motion1 = motion1;
  node.motion2 = motion2;

  do
  {
    Matrix3f R1_t, R2_t;
    Vec3f T1_t, T2_t;

    node.motion1->getCurrentTransform(R1_t, T1_t);
    node.motion2->getCurrentTransform(R2_t, T2_t);

    // compute the transformation from 1 to 2
    relativeTransform(R1_t, T1_t, R2_t, T2_t, node.R, node.T);

    node.delta_t = 1;
    node.min_distance = std::numeric_limits<FCL_REAL>::max();

    distanceRecurse(&node, 0, 0, NULL);

    if(node.delta_t <= node.t_err)
    {
      // std::cout << node.delta_t << " " << node.t_err << std::endl;
      break;
    }

    node.toc += node.delta_t;
    if(node.toc > 1)
    {
      node.toc = 1;
      break;
    }

    node.motion1->integrate(node.toc);
    node.motion2->integrate(node.toc);
  }
  while(1);

  toc = node.toc;

  if(node.toc < 1)
    return 1;

  return 0;
}


template
int conservativeAdvancement<RSS, MeshConservativeAdvancementTraversalNodeRSS, MeshCollisionTraversalNodeRSS>(const CollisionGeometry* o1,
                                                                                                             const MotionBase* motion1,
                                                                                                             const CollisionGeometry* o2,
                                                                                                             const MotionBase* motion2,
                                                                                                             const CollisionRequest& request,
                                                                                                             CollisionResult& result,
                                                                                                             FCL_REAL& toc);

template int conservativeAdvancement<OBBRSS, MeshConservativeAdvancementTraversalNodeOBBRSS, MeshCollisionTraversalNodeOBBRSS>(const CollisionGeometry* o1,
                                                                                                                               const MotionBase* motion1,
                                                                                                                               const CollisionGeometry* o2,
                                                                                                                               const MotionBase* motion2,
                                                                                                                               const CollisionRequest& request,
                                                                                                                               CollisionResult& result,
                                                                                                                               FCL_REAL& toc);

}
