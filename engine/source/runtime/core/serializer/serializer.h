#pragma once

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <filesystem>
#include <rttr/registration>
#include <string>

namespace Vain {

class Serializer {
  public:
    static std::string write(const rttr::instance &obj);
    static bool read(rttr::instance obj, const std::string &json);

  private:
    static bool isAtomic(const rttr::type &t);

    static bool writeAtomicType(
        const rttr::type &t,
        const rttr::variant &var,
        rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
    );

    static bool writeVariant(
        const rttr::variant &var, rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
    );

    static void writeArray(
        const rttr::variant_sequential_view &view,
        rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
    );

    static void writeAssociativeContainer(
        const rttr::variant_associative_view &view,
        rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
    );

    static void writeObject(
        const rttr::instance &obj2,
        rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
    );

    static rttr::variant readAtomicType(const rapidjson::Value &json_value);

    static rttr::variant readValue(
        rapidjson::Value::ConstMemberIterator &itr, const rttr::type &t
    );

    static void readArray(
        rttr::variant_sequential_view &view, const rapidjson::Value &json_array_value
    );

    static void readAssociativeContainer(
        rttr::variant_associative_view &view, const rapidjson::Value &json_array_value
    );

    static void readObject(rttr::instance obj2, const rapidjson::Value &json_object);
};

}  // namespace Vain