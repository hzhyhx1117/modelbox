/*
 * Copyright 2021 The Modelbox Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MODELBOX_GRAPH_CHECKER_H
#define MODELBOX_GRAPH_CHECKER_H

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "modelbox/base/status.h"
#include "modelbox/node.h"
#include "modelbox/virtual_node.h"

namespace modelbox {

class Graph;
class NodeBase;
class InputVirtualNode;
class OutputVirtualNode;
class OutputUnmatchVirtualNode;

using NodeStreamConnection =
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>;

enum OHC_Type { BEGIN = 0, END, MID };
enum Color_Level { NOT_CHANGE = 0, NEXT_LEVEL, PRE_LEVEL, SAME_LEVEL };
class LeastCommonAncestor {
 public:
  LeastCommonAncestor(
      const std::vector<std::shared_ptr<NodeBase>> &nodes,
      const std::unordered_map<std::string, std::shared_ptr<NodeBase>>
          &all_nodes);
  virtual ~LeastCommonAncestor();
  void Update(const std::vector<std::pair<std::string, std::string>> &values,
              const std::unordered_map<std::string, std::string> &match_map);
  std::pair<std::string, std::string> Find(const std::string &node_a,
                                           const std::string &node_b);

 private:
  void InitMap();
  std::string GetMatchPortName(int match_a, int match_b, int match_node);

 private:
  std::vector<std::shared_ptr<NodeBase>> nodes_;
  std::unordered_map<std::string, std::shared_ptr<NodeBase>> all_nodes_;

  int nodes_num_;
  std::map<int, std::vector<int>> paths_;

  std::unordered_map<int, std::string> index_name_map_;
  std::unordered_map<std::string, int> name_index_map_;
};

class OverHierarchyCheck {
 public:
  OverHierarchyCheck(
      const std::unordered_map<std::string, std::shared_ptr<NodeBase>>
          &all_nodes,
      const std::set<std::shared_ptr<NodeBase>> &start_nodes,
      const std::map<std::string, std::string> &loop_links_,
      const std::map<std::shared_ptr<OutPort>,
                     std::set<std::shared_ptr<InPort>>> &edges);
  virtual ~OverHierarchyCheck();

  Status Check(
      const std::unordered_map<std::string, std::string> &graph_match_map);

 private:
  void GetConnectNodes(std::shared_ptr<Node> node,
                       std::set<std::shared_ptr<Node>> &connect_nodes);
  Status GetNewColor(std::vector<int> &color, Color_Level &level);
  void GetNodeType(
      std::shared_ptr<Node> connect_node, OHC_Type &ohc_type,
      const std::unordered_map<std::string, std::string> &graph_match_map);
  Color_Level GetColorLevel(const OHC_Type &origin_type,
                            const OHC_Type &connect_type);

 private:
  std::unordered_map<std::string, std::shared_ptr<NodeBase>> all_nodes_;
  std::set<std::shared_ptr<NodeBase>> start_nodes_;
  std::map<std::string, std::string> loop_links_;
  std::map<std::shared_ptr<OutPort>, std::set<std::shared_ptr<InPort>>> edges_;
  std::unordered_map<std::string, std::vector<int>> color_map_;
  std::unordered_map<std::string, std::unordered_map<std::string, bool>> visited_;
  int max_color_{0};
};

class GraphChecker {
 public:
  GraphChecker(const std::vector<std::shared_ptr<NodeBase>> &nodes,
               const std::set<std::shared_ptr<NodeBase>> &start_nodes,
               const std::map<std::string, std::string> &loop_links,
               const std::map<std::shared_ptr<OutPort>,
                              std::set<std::shared_ptr<InPort>>> &edges);
  virtual ~GraphChecker();

  void SetMatchNodes();
  modelbox::Status Check();

 private:
  modelbox::Status CalNodeStreamMap(std::shared_ptr<NodeBase> node,
                                    NodeStreamConnection &node_stream_map);
  modelbox::Status CheckNodeMatch(std::shared_ptr<Node> node,
                                  const NodeStreamConnection &node_stream_map);
  modelbox::Status CheckCollapseMatch(std::shared_ptr<Node> node);
  modelbox::Status CheckBranchPathMatch(const std::string &start,
                                        const std::string &end);
  modelbox::Status CheckOverHierarchyMatch();
  modelbox::Status CheckUnmatchExpands(size_t size);
  bool IsNMNode(const std::shared_ptr<Node> &node);
  std::pair<std::string, std::string> LeastCommonAncestors(
      const std::vector<std::pair<std::string, std::string>> &match_nodes);
  std::unordered_map<std::string, std::string> GetGraphMatchMap();
  bool CheckPortMatch(const std::pair<std::string, std::string> &match_pair);
  void FindNearestNeighborMatchExpand(const std::string &node,
                                      std::string &match_node);
  void UpdateAncestorPath(
      const std::vector<std::pair<std::string, std::string>> &value);

 private:
  std::vector<std::shared_ptr<NodeBase>> nodes_;
  std::map<std::string, std::string> loop_links_;
  std::shared_ptr<LeastCommonAncestor> lca_;
  std::shared_ptr<OverHierarchyCheck> ovc_;
  std::unordered_map<std::string, std::shared_ptr<NodeBase>> all_nodes_;
  std::unordered_map<std::string, std::string> graph_match_map_;
  std::map<std::string, NodeStreamConnection> node_stream_connection_map_;
  size_t expands_{0};
};

}  // namespace modelbox

#endif