#ifndef LUA_WRAPPER_GENERATOR_TESTING_COMMENT_PARSER_HPP
#define LUA_WRAPPER_GENERATOR_TESTING_COMMENT_PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>

namespace RC::LuaWrapperGenerator
{
    struct CommentAttributeContainer
    {
        std::string name{};
        std::vector<std::string> params{};

        CommentAttributeContainer(std::string_view attribute_name) : name(attribute_name) {}
    };

    class CommentAttribute
    {
    private:
        const CommentAttributeContainer* attribute{};

    public:
        CommentAttribute() = default;
        CommentAttribute(const CommentAttributeContainer* attribute_container) : attribute(attribute_container) {}

    public:
        auto exists() const -> bool { return attribute; }
        auto get() const -> const CommentAttributeContainer& { return *attribute; }
        auto num_params() const -> size_t { return get().params.size(); }
        auto get_param(size_t param) -> const std::string&;
        auto has_param(size_t param) -> bool { return param < get().params.size(); }
    };

    class CommentParser
    {
    private:
        const std::string& m_comment;

        using CommentAttributes = std::unordered_map<std::string, CommentAttributeContainer>;
        CommentAttributes m_attributes{};

    public:
        explicit CommentParser(const std::string& comment);

    private:
        auto parse() -> void;

    public:
        auto get_attribute(const std::string& attribute_name) const -> const CommentAttribute;
    };
}

#endif //LUA_WRAPPER_GENERATOR_TESTING_COMMENT_PARSER_HPP
