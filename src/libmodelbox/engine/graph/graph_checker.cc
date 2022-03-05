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

#include "modelbox/graph_checker.h"

#include <cmath>
#include <queue>
#include <stack>

namespace modelbox {

void LeastCommonAncestor::InitMap() {
  int index = 0;
  for (auto &node : nodes_) {
    index_name_map_[index] = node->GetName();
    name_index_map_[node->GetName()] = index;
    index++;
  }
}

LeastCommonAncestor::LeastCommonAncestor(
    const std::vector<std::shared_ptr<NodeBase>> &nodes,
    const std::unordered_map<std::string, std::shared_ptr<NodeBase>> &all_nodes)
    : nodes_(nodes), all_nodes_(all_nodes) {
  InitMap();
}

LeastCommonAncestor::~LeastCommonAncestor() {
  index_name_map_.clear();
  name_index_map_.clear();
}

void LeastCommonAncestor::Update(
    const std::vector<std::pair<std::string, std::string>> &values,
    const std::unordered_map<std::string, std::string> &match_map) {
  for (auto &value : values) {
    auto cur_name = value.first;
    auto index = name_index_map_[cur_name];
    std::vector<int> path{index};
    auto pre_name = match_map.at(cur_name);
    while (pre_name != "external") {
      path.push_back(name_index_map_[pre_name]);
      cur_name = pre_name;
      pre_name = match_map.at(cur_name);
    }

    path.push_back(-1);
    paths_[index] = path;
  }
}

std::string LeastCommonAncestor::GetMatchPortName(int match_a, int match_b,
                                                  int match_node) {
  std::shared_ptr<Node> node_a =
      std::dynamic_pointer_cast<Node>(all_nodes_[index_name_map_[match_a]]);
  std::shared_ptr<Node> node_b =
      std::dynamic_pointer_cast<Node>(all_nodes_[index_name_map_[match_b]]);
  std::shared_ptr<Node> match =
      std::dynamic_pointer_cast<Node>(all_nodes_[index_name_map_[match_node]]);
  std::shared_ptr<InputVirtualNode> match_virtual_node;
  if (match == nullptr) {
    match_virtual_node = std::dynamic_pointer_cast<InputVirtualNode>(
        all_nodes_[index_name_map_[match_node]]);
  }

  auto input_port_a = node_a->GetInputPorts()[0];
  auto input_port_b = node_b->GetInputPorts()[0];
  std::string output_port_a =
      input_port_a->GetAllOutPort()[0].lock()->GetName();
  std::string output_port_b =
      input_port_b->GetAllOutPort()[0].lock()->GetName();
  if (output_port_a != output_port_b) {
    std::vector<std::shared_ptr<InPort>> match_input_ports;
    if (match == nullptr) {
      match_input_ports = match_virtual_node->GetInputPorts();
    } else {
      match_input_ports = match->GetInputPorts();
    }

    if (match_input_ports.size() == 0) {
      return "external";
    }
    return match_input_ports[0]->GetName();
  }

  return output_port_a;
}

std::pair<std::string, std::string> LeastCommonAncestor::Find(
    const std::string &node_a, const std::string &node_b) {
  if (node_a == node_b) {
    auto port_name = std::dynamic_pointer_cast<Node>(all_nodes_[node_a])
                         ->GetOutputPorts()[0]
                         ->GetName();
    return std::pair<std::string, std::string>(node_a, port_name);
  }

  int index_a = name_index_map_[node_a];
  int index_b = name_index_map_[node_b];
  auto path_a = paths_[index_a];
  auto path_b = paths_[index_b];
  int res = -1;
  if (path_a.size() > path_b.size()) {
    std::swap(path_a, path_b);
  }

  int begin_b = path_b.size() - path_a.size();
  size_t index = -1;
  for (size_t i = 0; i < path_a.size(); ++i) {
    if (path_a[i] != path_b[begin_b + i]) {
      continue;
    }

    res = path_a[i];
    index = i;
    break;
  }

  std::pair<std::string, std::string> ans;
  if (res == -1) {
    return ans;
  }

  std::string match_node_name = index_name_map_[res];
  if (index == 0) {
    auto matching_node =
        std::dynamic_pointer_cast<Node>(all_nodes_[match_node_name]);
    std::string match_port_name;
    if (matching_node->GetInputPorts().size() == 0) {
      match_port_name = "external";
    } else {
      match_port_name = matching_node->GetInputPorts()[0]->GetName();
    }

    ans = std::make_pair(match_node_name, match_port_name);
    return ans;
  }

  std::string match_port_name =
      GetMatchPortName(path_a[index - 1], path_b[begin_b + index - 1], res);
  ans = std::make_pair(match_node_name, match_port_name);
  return ans;
}

OverHierarchyCheck::OverHierarchyCheck(
    const std::unordered_map<std::string, std::shared_ptr<NodeBase>> &all_nodes,
    const std::set<std::shared_ptr<NodeBase>> &start_nodes,
    const std::map<std::string, std::string> &loop_links,
    const std::map<std::shared_ptr<OutPort>, std::set<std::shared_ptr<InPort>>>
        &edges)
    : all_nodes_(all_nodes),
      start_nodes_(start_nodes),
      loop_links_(loop_links),
      edges_(edges) {
        // for (auto &item: loop_links_) {
        //   MBLOG_ERROR << item.first << ", " << item.second;
        // }
      }

OverHierarchyCheck::~OverHierarchyCheck() {
  // all_nodes_.clear();
  // loop_links_.clear();
  // start_nodes_.clear();
  // edges_.clear();
}

void OverHierarchyCheck::GetConnectNodes(
    std::shared_ptr<Node> node,
    std::set<std::shared_ptr<Node>> &connect_nodes) {
  auto outports = node->GetOutputPorts();
  for (auto port : outports) {
    auto linkPorts = edges_[port];
    for (auto linkport : linkPorts) {
      connect_nodes.insert(
          std::dynamic_pointer_cast<Node>(linkport->GetNode()));
    }
  }
}

Status OverHierarchyCheck::GetNewColor(std::vector<int> &color,
                                       Color_Level &level) {
  switch (level) {
    case Color_Level::NOT_CHANGE:  // not change
      break;
    case Color_Level::NEXT_LEVEL:  // next level
      max_color_++;
      if (max_color_ <= color.back()) {
        return {STATUS_FAULT,
                "get color failed, new color is less than origin."};
      }
      color.push_back(max_color_);
      break;
    case Color_Level::PRE_LEVEL:  // pre level
      color.pop_back();
      break;
    case Color_Level::SAME_LEVEL:  // same level
      max_color_++;
      if (max_color_ <= color.back()) {
        return {STATUS_FAULT,
                "get color failed, new color is less than origin."};
      }
      color.pop_back();
      color.push_back(max_color_);
      break;
    default:
      return {STATUS_FAULT, "get new color failed, unsupport level."};
  }

  return STATUS_OK;
}

void OverHierarchyCheck::GetNodeType(
    std::shared_ptr<Node> connect_node, OHC_Type &ohc_type,
    const std::unordered_map<std::string, std::string> &graph_match_map) {
  if (connect_node->GetConditionType() == IF_ELSE ||
      connect_node->GetOutputType() == EXPAND ||
      connect_node->GetLoopType() == LOOP) {
    ohc_type = OHC_Type::BEGIN;
    return;
  }

  if (connect_node->GetOutputType() == COLLAPSE) {
    ohc_type = OHC_Type::END;
    return;
  }

  auto input_ports = connect_node->GetInputPorts();
  for (auto &input_port : input_ports) {
    auto connect_ports = input_port->GetAllOutPort();
    if (connect_ports.size() <= 1) {
      continue;
    }

    // if (connect_ports.size() == 1) {
    //   // auto pre_node = connect_ports[0].lock()->GetNode();
    //   // auto pre_real_node = std::dynamic_pointer_cast<Node>(pre_node);
    //   // if (pre_real_node->GetLoopType() != LOOP) {
    //   //   continue;
    //   // }

    //   // if (loop_links_[pre_node->GetName()] != connect_node->GetName()) {
    //   //   continue;
    //   // }

    //   // ohc_type = OHC_Type::END;
    //   // return;
    // }

    auto pre_match_node =
        all_nodes_[graph_match_map.at(connect_node->GetName())];
    auto pre_real_match_node = std::dynamic_pointer_cast<Node>(pre_match_node);
    if (pre_real_match_node->GetConditionType() != IF_ELSE) {
      continue;
    }

    ohc_type = OHC_Type::END;
    return;
  }

  ohc_type = OHC_Type::MID;
}

Color_Level OverHierarchyCheck::GetColorLevel(const OHC_Type &origin_type,
                                              const OHC_Type &connect_type) {
  if (origin_type == OHC_Type::END && connect_type == OHC_Type::BEGIN) {
    return Color_Level::SAME_LEVEL;
  }

  if ((origin_type == OHC_Type::END && connect_type == OHC_Type::END) ||
      (origin_type == OHC_Type::END && connect_type == OHC_Type::MID)) {
    return Color_Level::PRE_LEVEL;
  }

  if ((origin_type == OHC_Type::BEGIN && connect_type == OHC_Type::BEGIN) ||
      (origin_type == OHC_Type::MID && connect_type == OHC_Type::BEGIN)) {
    return Color_Level::NEXT_LEVEL;
  }

  return Color_Level::NOT_CHANGE;
}

Status OverHierarchyCheck::Check(
    const std::unordered_map<std::string, std::string> &graph_match_map) {
  Status status{STATUS_OK};
  for (auto &start_node : start_nodes_) {
    auto real_node = std::dynamic_pointer_cast<Node>(start_node);
    if (real_node == nullptr) {
      continue;
    }

    std::stack<std::shared_ptr<Node>> stack;
    stack.push(real_node);
    color_map_[real_node->GetName()] = {0};
    while (!stack.empty()) {
      auto node = stack.top();
      stack.pop();
      OHC_Type origin_type;
      GetNodeType(node, origin_type, graph_match_map);

      std::set<std::shared_ptr<Node>> connect_nodes;
      GetConnectNodes(node, connect_nodes);
      for (auto &connect_node : connect_nodes) {
        auto real_origin_type  = origin_type;
        if (visited_[node->GetName()][connect_node->GetName()]) {
          continue;
        }

        visited_[node->GetName()][connect_node->GetName()] = true;
        if (node->GetLoopType() == LOOP &&
            loop_links_[node->GetName()] != connect_node->GetName()) {
          real_origin_type = OHC_Type::END;
        }

        if (connect_node->GetLoopType() == LOOP &&
            loop_links_[connect_node->GetName()] == node->GetName()) {
          continue;
        }

        stack.push(connect_node);

        OHC_Type connect_type;
        GetNodeType(connect_node, connect_type, graph_match_map);

        auto level = GetColorLevel(real_origin_type, connect_type);

        auto color = color_map_[node->GetName()];
        status = GetNewColor(color, level);
        if (status != STATUS_OK) {
          return {status, "get new color failed."};
        }

        if (color_map_.find(connect_node->GetName()) == color_map_.end()) {
          color_map_[connect_node->GetName()] = color;
          continue;
        }

        if (color_map_[connect_node->GetName()] != color) {
          status = {STATUS_FAULT, node->GetName() + " link " +
                                      connect_node->GetName() +
                                      " is illegal. "};
          return status;
        }
      }
    }
  }
  return status;
}

GraphChecker::GraphChecker(
    const std::vector<std::shared_ptr<NodeBase>> &nodes,
    const std::set<std::shared_ptr<NodeBase>> &start_nodes,
    const std::map<std::string, std::string> &loop_links,
    const std::map<std::shared_ptr<OutPort>, std::set<std::shared_ptr<InPort>>>
        &edges)
    : nodes_(nodes), loop_links_(loop_links) {
  for (auto node : nodes) {
    all_nodes_[node->GetName()] = node;
    MBLOG_INFO << "name: " << node->GetName() << ", node: " << node;
  }

  lca_ = std::make_shared<LeastCommonAncestor>(nodes_, all_nodes_);

  ovc_ = std::make_shared<OverHierarchyCheck>(all_nodes_, start_nodes,
                                              loop_links_, edges);
}

GraphChecker::~GraphChecker() {
  all_nodes_.clear();
  lca_ = nullptr;
  ovc_ = nullptr;
}

void GraphChecker::SetMatchNodes() {
  for (auto &node : all_nodes_) {
    auto real_node = std::dynamic_pointer_cast<Node>(node.second);
    if (real_node == nullptr) {
      continue;
    }

    if (real_node->GetInputNum() <= 1) {
      continue;
    }

    auto match_node = std::dynamic_pointer_cast<Node>(
        all_nodes_.at(graph_match_map_[node.first]));
    real_node->SetMatchNode(match_node);
  }
}

Status GraphChecker::Check() {
  for (auto &check_node : nodes_) {
    NodeStreamConnection node_stream_map;
    auto status = CalNodeStreamMap(check_node, node_stream_map);
    if (status != STATUS_SUCCESS) {
      auto msg = "cal node stream map failed";
      MBLOG_ERROR << msg;
      return {status, msg};
    }

    auto cur_real_node = std::dynamic_pointer_cast<Node>(check_node);
    // virtual node
    if (cur_real_node == nullptr) {
      continue;
    }

    status = CheckNodeMatch(cur_real_node, node_stream_map);
    if (status != STATUS_SUCCESS) {
      auto msg = "check single node match failed";
      MBLOG_ERROR << msg << ", " << status.WrapErrormsgs();
      return {status, msg};
    }

    status = CheckCollapseMatch(cur_real_node);
    if (status != STATUS_SUCCESS) {
      auto msg = "check single node collapse match failed";
      MBLOG_ERROR << msg << ", " << status.WrapErrormsgs();
      return {status, msg};
    }

    node_stream_connection_map_[cur_real_node->GetName()] = node_stream_map;
  }

  auto status = CheckOverHierarchyMatch();
  if (status != STATUS_SUCCESS) {
    auto msg = "check over hierarchy match failed";
    MBLOG_ERROR << msg;
    return {status, msg};
  }

  return STATUS_SUCCESS;
}

Status GraphChecker::CalNodeStreamMap(std::shared_ptr<NodeBase> node,
                                      NodeStreamConnection &node_stream_map) {
  Status status{STATUS_SUCCESS};
  auto input_ports = node->GetInputPorts();

  // no input
  if (input_ports.empty()) {
    auto external =
        std::make_pair<std::string, std::string>("external", "external");
    node_stream_map["p1"] = {external};
    graph_match_map_[node->GetName()] = "external";
    return status;
  }

  for (auto &input_port : input_ports) {
    auto pre_output_ports = input_port->GetAllOutPort();
    for (auto &pre_output_port : pre_output_ports) {
      auto key = input_port->GetName();
      std::string output_port_name = pre_output_port.lock()->GetName();
      std::string pre_node_name = pre_output_port.lock()->GetNode()->GetName();
      auto value =
          std::pair<std::string, std::string>(pre_node_name, output_port_name);
      if (node_stream_map.find(key) == node_stream_map.end()) {
        node_stream_map[key] = {value};
        continue;
      }

      node_stream_map[key].emplace_back(value);
    }
  }

  if (node_stream_map.empty()) {
    status = {STATUS_FAULT, "cal node stream connection failed."};
  }

  return status;
}

bool GraphChecker::IsNMNode(const std::shared_ptr<Node> &node) {
  return node->GetBatchSize() == 1 ? false : true;
}

Status GraphChecker::CheckBranchPathMatch(const std::string &start,
                                          const std::string &end) {
  Status status{STATUS_SUCCESS};
  int expand_collapse_flag = 0;
  if (end == start) {
    return status;
  }

  std::string tmp{start};
  do {
    auto tmp_node = std::dynamic_pointer_cast<Node>(all_nodes_[tmp]);

    if (tmp_node == nullptr) {
      break;
    }
    // check whether n:m node
    if (tmp != end && IsNMNode(tmp_node)) {
      status = {STATUS_FAULT, "can't make n:m node:" + tmp + " in a branch."};
      return status;
    }

    if (tmp_node->GetOutputType() == COLLAPSE) {
      expand_collapse_flag++;
    }

    if (tmp_node->GetOutputType() == EXPAND) {
      expand_collapse_flag--;
    }

    tmp = graph_match_map_[tmp];
  } while (tmp != graph_match_map_[end]);

  auto end_node = std::dynamic_pointer_cast<Node>(all_nodes_[end]);
  if (end_node == nullptr && expand_collapse_flag != 0) {
    status = {STATUS_FAULT, "expand collapse match failed in path from node:" +
                                start + " to node:" + end};
    return status;
  }

  if (expand_collapse_flag == -1 && end_node->GetOutputType() == EXPAND) {
    expands_++;
    return status;
  }

  if (expand_collapse_flag != 0) {
    status = {STATUS_FAULT, "expand collapse match failed in path from node:" +
                                start + " to node:" + end};
  }

  return status;
}

bool GraphChecker::CheckPortMatch(
    const std::pair<std::string, std::string> &match_pair) {
  std::shared_ptr<Node> node =
      std::dynamic_pointer_cast<Node>(all_nodes_[match_pair.first]);
  auto port = node->GetOutputPort(match_pair.second);

  // input port
  if (port == nullptr) {
    return false;
  }

  // output port
  return true;
}

void GraphChecker::UpdateAncestorPath(
    const std::vector<std::pair<std::string, std::string>> &values) {
  lca_->Update(values, graph_match_map_);
}

Status GraphChecker::CheckUnmatchExpands(size_t size) {
  if (expands_ == size || expands_ == 0) {
    expands_ = 0;
    return STATUS_OK;
  }

  expands_ = 0;
  return {STATUS_FAULT, "unmatch expands are not the same."};
}

Status GraphChecker::CheckNodeMatch(
    std::shared_ptr<Node> node, const NodeStreamConnection &node_stream_map) {
  Status status{STATUS_SUCCESS};
  auto node_name = node->GetName();
  if (node->GetInputPorts().empty()) {
    graph_match_map_[node_name] = "external";
    return status;
  }

  std::vector<std::pair<std::string, std::string>> single_match_result;
  for (auto iter = node_stream_map.begin(); iter != node_stream_map.end();
       ++iter) {
    auto values = iter->second;

    // in: {d.output}
    if (values.size() == 1) {
      single_match_result.emplace_back(values[0]);
      graph_match_map_[node_name] = values[0].first;
      UpdateAncestorPath(values);
      continue;
    }

    // in: {d.output, e.output}
    if (node->GetLoopType() == LOOP) {
      if (values.size() != 2) {
        status = {STATUS_FAULT, "loop node can only link 2 edges."};
        return status;
      }

      for (auto &loop_link : loop_links_) {
        if (loop_link.first != node_name) {
          continue;
        }

        for (auto &value : values) {
          if (value.first == loop_link.second) {
            continue;
          }

          single_match_result.emplace_back(value);
          graph_match_map_[node_name] = value.first;
          return status;
        }
      }
    }

    UpdateAncestorPath(values);
    auto single_match_node = LeastCommonAncestors(values);
    if (single_match_node.first.empty() && single_match_node.second.empty()) {
      status = {STATUS_FAULT,
                node_name + " : " + iter->first + " port match failed."};
      return status;
    }

    // true: output port; false: input port
    // scene 2)
    if (CheckPortMatch(single_match_node)) {
      status = {STATUS_FAULT, node_name + " and " + single_match_node.first +
                                  " can not match."};
      return status;
    }

    // scene 4)
    auto single_match_real_node =
        std::dynamic_pointer_cast<Node>(all_nodes_[single_match_node.first]);
    if (single_match_real_node != nullptr &&
        single_match_real_node->GetConditionType() != IF_ELSE) {
      status = {STATUS_FAULT, node_name + " and " + single_match_node.first +
                                  " can not match. single_match_node.first "
                                  "should be set a condition node."};
      return status;
    }

    for (auto &value : values) {
      status = CheckBranchPathMatch(value.first, single_match_node.first);
      if (status != STATUS_SUCCESS) {
        return status;
      }
    }

    if (!CheckUnmatchExpands(values.size())) {
      status = {STATUS_FAULT, "branch have unmatched expand node."};
      return status;
    }

    graph_match_map_[node_name] = single_match_real_node->GetName();
    single_match_result.emplace_back(single_match_node);
  }

  if (single_match_result.size() == 1) {
    graph_match_map_[node_name] = node_stream_map.begin()->second[0].first;
    return status;
  }

  UpdateAncestorPath(single_match_result);
  auto multi_match_node = LeastCommonAncestors(single_match_result);
  if (multi_match_node.first.empty() && multi_match_node.second.empty()) {
    status = {STATUS_FAULT, node_name + " node match failed."};
  }

  auto multi_match_real_node =
      std::dynamic_pointer_cast<Node>(all_nodes_[multi_match_node.first]);

  if (multi_match_real_node != nullptr &&
      multi_match_real_node->GetConditionType() == IF_ELSE) {
    status = {STATUS_FAULT, "multi match node can not be if-else node."};
    return status;
  }

  for (auto &single_match : single_match_result) {
    status = CheckBranchPathMatch(single_match.first, multi_match_node.first);
    if (status != STATUS_SUCCESS) {
      return status;
    }
  }

  if (!CheckUnmatchExpands(single_match_result.size())) {
    status = {STATUS_FAULT, "branch have unmatched expand node."};
    return status;
  }

  // scene 5) 6) 7) 8)
  graph_match_map_[node_name] = all_nodes_[multi_match_node.first]->GetName();
  return status;
}

Status GraphChecker::CheckOverHierarchyMatch() {
  return ovc_->Check(graph_match_map_);
}

void GraphChecker::FindNearestNeighborMatchExpand(const std::string &node,
                                                  std::string &match_node) {
  int expand_collapse_flag = 1;
  std::string tmp{node}, pre_node_name;
  std::shared_ptr<Node> pre_node;
  while (true) {
    pre_node_name = graph_match_map_[tmp];

    if (!pre_node_name.empty() && pre_node_name == "external") {
      break;
    }

    pre_node = std::dynamic_pointer_cast<Node>(all_nodes_[pre_node_name]);

    if (pre_node->GetOutputType() == COLLAPSE) {
      expand_collapse_flag++;
    }

    if (pre_node->GetOutputType() == EXPAND) {
      expand_collapse_flag--;
    }

    if (expand_collapse_flag == 0) {
      break;
    }

    tmp = pre_node_name;
  };

  if (expand_collapse_flag != 0) {
    return;
  }

  match_node = pre_node_name;
  graph_match_map_[node] = pre_node_name;
  return;
}

Status GraphChecker::CheckCollapseMatch(std::shared_ptr<Node> node) {
  Status status{STATUS_SUCCESS};
  if (node->GetInputNum() == 0) {
    return status;
  }

  if (node->GetOutputType() != COLLAPSE) {
    return status;
  }

  std::string match_node;
  FindNearestNeighborMatchExpand(node->GetName(), match_node);
  if (match_node.empty()) {
    status = {STATUS_FAULT,
              "can't find node:" + node->GetName() + " match expand node."};
    return status;
  }

  return status;
}

std::unordered_map<std::string, std::string> GraphChecker::GetGraphMatchMap() {
  return graph_match_map_;
}

std::pair<std::string, std::string> GraphChecker::LeastCommonAncestors(
    const std::vector<std::pair<std::string, std::string>> &match_nodes) {
  auto res_node = match_nodes[0];
  for (size_t i = 1; i < match_nodes.size(); ++i) {
    auto res = lca_->Find(res_node.first, match_nodes[i].first);
    res_node = res;
    if (res_node.first.empty() && res_node.second.empty()) {
      break;
    }
  }

  return res_node;
}

}  // namespace modelbox
