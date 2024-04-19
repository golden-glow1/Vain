#include "serializer.h"

#include <iostream>

namespace Vain {

std::string Serializer::write(const rttr::instance &obj) {
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

    writeObject(obj, writer);

    return buffer.GetString();
}

bool Serializer::read(rttr::instance obj, const std::string &json) {
    rapidjson::Document doc;

    if (doc.Parse(json.c_str()).HasParseError()) {
        return false;
    }

    readObject(obj, doc);

    return true;
}

bool Serializer::isAtomic(const rttr::type &t) {
    return t.is_arithmetic() || t.is_enumeration() || t == rttr::type::get<std::string>();
}

bool Serializer::writeAtomicType(
    const rttr::type &t,
    const rttr::variant &var,
    rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
) {
    using namespace rttr;

    if (t.is_arithmetic()) {
        if (t == type::get<bool>()) {
            writer.Bool(var.to_bool());
        } else if (t == type::get<int8_t>()) {
            writer.Int(var.to_int8());
        } else if (t == type::get<uint8_t>()) {
            writer.Uint(var.to_uint8());
        } else if (t == type::get<int16_t>()) {
            writer.Int(var.to_int16());
        } else if (t == type::get<uint16_t>()) {
            writer.Uint(var.to_uint16());
        } else if (t == type::get<int32_t>()) {
            writer.Int(var.to_int32());
        } else if (t == type::get<uint32_t>()) {
            writer.Uint(var.to_uint32());
        } else if (t == type::get<int64_t>()) {
            writer.Int64(var.to_int64());
        } else if (t == type::get<uint64_t>()) {
            writer.Uint64(var.to_uint64());
        } else if (t == type::get<float>()) {
            writer.Double(var.to_float());
        } else if (t == type::get<double>()) {
            writer.Double(var.to_double());
        }

        return true;
    } else if (t.is_enumeration()) {
        if (var.can_convert<std::string>()) {
            writer.String(var.to_string().c_str());
            return true;
        }
        if (var.can_convert<uint64_t>()) {
            writer.Uint64(var.to_uint64());
            return true;
        }

        writer.Null();
        return true;
    } else if (t == type::get<std::string>()) {
        writer.String(var.to_string().c_str());
        return true;
    }

    return false;
}

bool Serializer::writeVariant(
    const rttr::variant &var, rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
) {
    using namespace rttr;

    auto value_type = var.get_type();
    auto wrapped_type =
        value_type.is_wrapper() ? value_type.get_wrapped_type() : value_type;
    bool is_wrapper = value_type != wrapped_type;

    if (isAtomic(wrapped_type)) {
        writeAtomicType(
            wrapped_type, is_wrapper ? var.extract_wrapped_value() : var, writer
        );
    } else if (var.is_sequential_container()) {
        writeArray(var.create_sequential_view(), writer);
    } else if (var.is_associative_container()) {
        writeAssociativeContainer(var.create_associative_view(), writer);
    } else {
        auto child_props = wrapped_type.get_properties();
        if (!child_props.empty()) {
            writeObject(var, writer);
        } else {
            bool ok = false;
            auto text = var.to_string(&ok);
            writer.String(text.c_str());
            if (!ok) {
                return false;
            }
        }
    }

    return true;
}

void Serializer::writeArray(
    const rttr::variant_sequential_view &view,
    rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
) {
    using namespace rttr;

    writer.StartArray();
    for (const auto &item : view) {
        if (item.is_sequential_container()) {
            writeArray(item.create_sequential_view(), writer);
        } else {
            variant wrapped_var = item.extract_wrapped_value();
            type value_type = wrapped_var.get_type();
            if (isAtomic(value_type)) {
                writeAtomicType(value_type, wrapped_var, writer);
            } else {
                writeObject(wrapped_var, writer);
            }
        }
    }
    writer.EndArray();
}

void Serializer::writeAssociativeContainer(
    const rttr::variant_associative_view &view,
    rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
) {
    using namespace rttr;

    static const string_view key_name("key");
    static const string_view value_name("value");

    writer.StartArray();

    if (view.is_key_only_type()) {
        for (auto &item : view) {
            writeVariant(item.first, writer);
        }
    } else {
        for (auto &item : view) {
            writer.StartObject();
            writer.String(
                key_name.data(),
                static_cast<rapidjson::SizeType>(key_name.length()),
                false
            );

            writeVariant(item.first, writer);

            writer.String(
                value_name.data(),
                static_cast<rapidjson::SizeType>(value_name.length()),
                false
            );

            writeVariant(item.second, writer);

            writer.EndObject();
        }
    }

    writer.EndArray();
}

void Serializer::writeObject(
    const rttr::instance &obj2, rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer
) {
    using namespace rttr;
    writer.StartObject();
    instance obj =
        obj2.get_type().get_raw_type().is_wrapper() ? obj2.get_wrapped_instance() : obj2;

    auto prop_list = obj.get_derived_type().get_properties();
    for (auto prop : prop_list) {
        if (prop.get_metadata("NO_SERIALIZE")) {
            continue;
        }

        variant prop_value = prop.get_value(obj);
        if (!prop_value) {
            continue;
        }

        const auto name = prop.get_name();
        writer.String(
            name.data(), static_cast<rapidjson::SizeType>(name.length()), false
        );
        if (!writeVariant(prop_value, writer)) {
            std::cerr << "cannot serialize property: " << name << std::endl;
        }
    }

    writer.EndObject();
}

rttr::variant Serializer::readAtomicType(const rapidjson::Value &json_value) {
    using namespace rapidjson;

    switch (json_value.GetType()) {
    case kStringType:
        return std::string{json_value.GetString()};
    case kNullType:
        return nullptr;
    case kFalseType:
    case kTrueType:
        return json_value.GetBool();
    case kNumberType:
        if (json_value.IsInt()) {
            return json_value.GetInt();
        } else if (json_value.IsUint()) {
            return json_value.GetUint();
        } else if (json_value.IsInt64()) {
            return json_value.GetInt64();
        } else if (json_value.IsUint64()) {
            return json_value.GetUint64();
        } else if (json_value.IsDouble()) {
            return json_value.GetDouble();
        }
    default:
        break;
    }

    return rttr::variant{};
}

rttr::variant Serializer::readValue(
    rapidjson::Value::ConstMemberIterator &itr, const rttr::type &t
) {
    using namespace rttr;

    auto &json_value = itr->value;
    variant value = readAtomicType(json_value);
    if (!value.convert(t)) {
        if (json_value.IsObject()) {
            constructor ctor = t.get_constructor();
            for (auto &item : t.get_constructors()) {
                if (item.get_instantiated_type() == t) {
                    ctor = item;
                }
            }
            value = ctor.invoke();
            readObject(value, json_value);
        }
    }

    return value;
}

void Serializer::readArray(
    rttr::variant_sequential_view &view, const rapidjson::Value &json_array_value
) {
    using namespace rttr;
    using namespace rapidjson;

    view.set_size(json_array_value.Size());
    for (SizeType i = 0; i < json_array_value.Size(); ++i) {
        auto &json_index_value = json_array_value[i];
        if (json_index_value.IsArray()) {
            auto sub_array_view = view.get_value(i).create_sequential_view();
            readArray(sub_array_view, json_index_value);
        } else if (json_index_value.IsObject()) {
            variant var_tmp = view.get_value(i);
            variant wrapped_var = var_tmp.extract_wrapped_value();
            readObject(wrapped_var, json_index_value);
            view.set_value(i, wrapped_var);
        } else {
            const type array_type = view.get_rank_type(i);
            variant extracted_value = readAtomicType(json_index_value);
            if (extracted_value.convert(array_type)) {
                view.set_value(i, extracted_value);
            }
        }
    }
}

void Serializer::readAssociativeContainer(
    rttr::variant_associative_view &view, const rapidjson::Value &json_array_value
) {
    using namespace rttr;
    using namespace rapidjson;

    for (rapidjson::SizeType i = 0; i < json_array_value.Size(); ++i) {
        auto &json_index_value = json_array_value[i];
        if (json_index_value.IsObject()) {
            auto key_itr = json_index_value.FindMember("key");
            auto value_itr = json_index_value.FindMember("value");

            if (key_itr != json_index_value.MemberEnd() &&
                value_itr != json_index_value.MemberEnd()) {
                auto key_var = readValue(key_itr, view.get_key_type());
                auto value_var = readValue(value_itr, view.get_value_type());
                if (key_var && value_var) {
                    view.insert(key_var, value_var);
                }
            }
        } else {
            variant extracted_value = readAtomicType(json_index_value);
            if (extracted_value && extracted_value.convert(view.get_key_type())) {
                view.insert(extracted_value);
            }
        }
    }
}

void Serializer::readObject(rttr::instance obj2, const rapidjson::Value &json_object) {
    using namespace rttr;
    using namespace rapidjson;

    instance obj =
        obj2.get_type().get_raw_type().is_wrapper() ? obj2.get_wrapped_instance() : obj2;
    const auto prop_list = obj.get_derived_type().get_properties();

    for (auto prop : prop_list) {
        auto ret = json_object.FindMember(prop.get_name().data());
        if (ret == json_object.MemberEnd()) continue;
        const type value_type = prop.get_type();

        auto &json_value = ret->value;
        switch (json_value.GetType()) {
        case kArrayType: {
            variant var;
            if (value_type.is_sequential_container()) {
                var = prop.get_value(obj);
                auto view = var.create_sequential_view();
                readArray(view, json_value);
            } else if (value_type.is_associative_container()) {
                var = prop.get_value(obj);
                auto associative_view = var.create_associative_view();
                readAssociativeContainer(associative_view, json_value);
            }

            prop.set_value(obj, var);
            break;
        }
        case kObjectType: {
            variant var = prop.get_value(obj);
            readObject(var, json_value);
            prop.set_value(obj, var);
            break;
        }
        default:
            variant extracted_value = readAtomicType(json_value);
            if (extracted_value.convert(value_type)) {
                prop.set_value(obj, extracted_value);
            }
        }
    }
}

}  // namespace Vain