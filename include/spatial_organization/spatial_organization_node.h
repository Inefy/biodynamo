#ifndef SPATIAL_ORGANIZATION_SPATIAL_ORGANIZATION_NODE_H_
#define SPATIAL_ORGANIZATION_SPATIAL_ORGANIZATION_NODE_H_

#include <string>
#include <list>
#include <array>
#include <memory>

#include "string_builder.h"
#include "spatial_organization/spatial_organization_node_movement_listener.h"

namespace cx3d {
namespace spatial_organization {

template<class T> class Edge;
template<class T> class SpaceNode;
template<class T> class SpatialOrganizationEdge;

/**
 * Interface to define the basic properties of a node in the triangulation.
 *
 * @param <T> The type of user objects associated with each node in the triangulation.
 */
template<class T>
class SpatialOrganizationNode {
 public:
  virtual ~SpatialOrganizationNode() {
  }

  virtual void addSpatialOrganizationNodeMovementListener(typename SpatialOrganizationNodeMovementListener<T>::UPtr listener) = 0;

  /**
   * Returns a list that allows to iterate over all edges
   * incident to this node.
   */
  virtual std::list<std::shared_ptr<Edge<T>> > getEdges() const = 0;  //TODO change to SpatialOrganizationEdge once porting has been finished

  virtual std::list<T*> getNeighbors() const = 0;

  // todo change to interface type
  virtual std::unique_ptr<SpaceNode<T>> getNewInstance(const std::array<double, 3>& position, T* user_object) = 0;

  virtual std::list<T*> getPermanentListOfNeighbors() const = 0;

  virtual std::array<double, 3> getPosition() const = 0;

  virtual T* getUserObject() const = 0;

  virtual std::array<T*, 4> getVerticesOfTheTetrahedronContaining(
      const std::array<double, 3>& position, std::array<int, 1>& returned_null) const = 0;

  virtual double getVolume() const = 0;

  virtual void moveFrom(const std::array<double, 3>& delta) = 0;

  virtual void remove() = 0;

  virtual std::string toString() const =0;
};

}  // namespace spatial_organization
}  // namespace cx3d

#endif  // SPATIAL_ORGANIZATION_SPATIAL_ORGANIZATION_NODE_H_
