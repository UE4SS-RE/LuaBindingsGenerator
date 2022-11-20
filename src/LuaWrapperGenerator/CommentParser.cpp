#include <stdexcept>

#include <LuaWrapperGenerator/CommentParser.hpp>

namespace RC::LuaWrapperGenerator
{
    auto CommentAttribute::get_param(size_t param) -> const std::string&
    {
        if (param >= get().params.size())
        {
            throw std::runtime_error{"Attempted to get a comment param but there are no comment params to get."};
        }

        return get().params[param];
    }

    CommentParser::CommentParser(const std::string& comment) : m_comment(comment)
    {
        parse();
    }

    auto CommentParser::parse() -> void
    {
        static constexpr std::string_view CustomAttributeTag{"CUSTOM_ATTRIBUTE["};
        auto attr_tag_start = m_comment.find(CustomAttributeTag);
        auto attr_tag_end = m_comment.find(']');
        if (attr_tag_start == m_comment.npos || attr_tag_end == m_comment.npos)
        {
            // Comment doesn't have a valid format
            return;
        }

        auto attr_start = attr_tag_start + CustomAttributeTag.size();
        auto attr_end = attr_tag_end;

        auto attr = m_comment.substr(attr_start, attr_end - attr_start);
        auto attr_name_start = attr.find('(');
        // If we have no ')' then we assume that the entire rest of the comment belongs to one attribute.
        size_t attr_name_last_end{};

        //size_t num_attributes_parsed = static_cast<size_t>(attr_name_end != attr.npos) + 1;

        do
        {
            auto attribute_name = attr_name_start != attr.npos ? attr.substr(attr_name_last_end, attr_name_start - attr_name_last_end) : attr;
            auto& attribute = m_attributes.emplace(attribute_name, attribute_name).first->second;

            auto attr_params_start = attr_name_start;
            if (attr_params_start != attr.npos && attr_params_start + 1 < attr.size())
            {
                ++attr_params_start;
                std::string param{};
                for (auto i = attr_params_start; i < attr.size(); ++i)
                {
                    if (attr[i] == ',' || attr[i] == ')')
                    {
                        attribute.params.emplace_back(param);
                        param.clear();

                        if (attr[i] == ')')
                        {
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    if (attr[i] == ' ') { continue; }

                    param.push_back(attr[i]);
                }
            }

            attr_name_last_end = attr.find(')', attr_name_last_end);
            attr_name_start = attr.find('(', attr_name_last_end);
            if (attr_name_start == attr.npos)
            {
                break;
            }
            else
            {
                for (; attr_name_last_end < attr.size() && (attr[attr_name_last_end] == ')' || attr[attr_name_last_end] == ',' || attr[attr_name_last_end] == ' '); ++attr_name_last_end) {}
            }
        }
        while (true);

        //printf_s("attr_tag_start: %i\n", attr_tag_start);
        //printf_s("attr_tag_end: %i\n", attr_tag_end);
        //printf_s("attr_start: %i\n", attr_start);
        //printf_s("attr_end: %i\n", attr_end);
        //printf_s("attr: %s\n", attr.c_str());
        //printf_s("attr_name: %s\n", m_attribute_name.c_str());
        //printf_s("params: %i (", m_params.size());
        //for (const auto& param : m_params)
        //{
        //    printf_s("%s, ", param.data());
        //}
        //printf_s(")\n");
    }

    auto CommentParser::get_attribute(const std::string& attribute_name) const -> const CommentAttribute
    {
        if (auto it = m_attributes.find(attribute_name); it != m_attributes.end())
        {
            return {&it->second};
        }
        else
        {
            return {nullptr};
        }
    }
}