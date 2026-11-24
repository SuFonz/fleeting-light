// 极简 JSON 值类型：解析 / 序列化 / 递归合并。
// 只依赖标准库，够配置文件和 IPC 消息用。
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fl {

class Json {
 public:
  enum class Type : unsigned char { Null, Bool, Number, String, Array, Object };

  Json();
  Json(std::nullptr_t);
  Json(bool v);
  Json(int v);
  Json(unsigned v);
  Json(long long v);
  Json(double v);
  Json(const char* v);
  Json(std::string v);

  static Json array();
  static Json object();
  static const Json& null();

  Type type() const { return type_; }
  bool is_null() const { return type_ == Type::Null; }
  bool is_bool() const { return type_ == Type::Bool; }
  bool is_number() const { return type_ == Type::Number; }
  bool is_string() const { return type_ == Type::String; }
  bool is_array() const { return type_ == Type::Array; }
  bool is_object() const { return type_ == Type::Object; }

  // ---- 取值（类型不匹配时返回给定默认值，永不抛异常） ----
  bool as_bool(bool fallback = false) const;
  double as_double(double fallback = 0.0) const;
  long long as_int(long long fallback = 0) const;
  const std::string& as_string() const;
  std::string as_string(std::string fallback) const;

  // ---- 对象访问 ----
  bool contains(std::string_view key) const;
  const Json& at(std::string_view key) const;
  // 非 const operator[] 会把 Null 自动变成 Object，便于 j["a"]["b"] = 1;
  Json& operator[](std::string_view key);
  const Json& operator[](std::string_view key) const;
  const std::map<std::string, Json, std::less<>>& members() const { return obj_; }
  void set(std::string key, Json value);
  bool erase(std::string_view key);

  // ---- 数组访问 ----
  std::size_t size() const;
  const Json& at_index(std::size_t index) const;
  const std::vector<Json>& elements() const { return arr_; }
  void push(Json value);

  // 点号路径访问：get_path("wallpaper.fps")
  const Json& get_path(std::string_view dotted_path) const;

  // 递归合并：对象逐 key 递归，其余类型整体替换（patch 里的 null 表示删除）
  void merge_patch(const Json& patch);

  // indent == 0 表示紧凑输出
  std::string dump(int indent = 0) const;
  static Json parse(std::string_view text, std::string* error = nullptr);

  bool operator==(const Json& other) const;
  bool operator!=(const Json& other) const { return !(*this == other); }

 private:
  void dump_to(std::string& out, int indent, int depth) const;

  Type type_ = Type::Null;
  bool bool_ = false;
  double num_ = 0.0;
  std::string str_;
  std::vector<Json> arr_;
  std::map<std::string, Json, std::less<>> obj_;
};

}  // namespace fl
