/*
MIT License

Copyright (c) 2023 Ezequias Moises dos Santos Silva

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once
#include <snot.h>

#include <cinttypes>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace snot
{

class value
{
public:
    enum type
    {
        string,
        decimal,
        octal,
        hexadecimal,
    };

    value() : m_type(string), m_value(""){};
    value(const std::string &value, type type = string)
        : m_type(type), m_value(value){};
    value(const char *value, type type = string)
        : m_type(type), m_value(value){};

    template <typename T,
              typename = typename std::enable_if<std::is_arithmetic<T>::value,
                                                 T>::type>
    value(T t, type type = decimal) : m_type(type), m_value(encode(type, t))
    {
    }

    operator const std::string &() const { return m_value; }

    template <
        typename T,
        typename U = typename std::enable_if<std::is_floating_point<T>::value ||
                                                 std::is_integral<T>::value,
                                             T>::type>
    explicit operator U() const noexcept
    {
        return get_value<U>();
    }

    constexpr type get_type() const { return m_type; }

    template <typename T,
              typename =
                  typename std::enable_if<!std::is_integral<T>::value, T>::type>
    T get_value(float = 0) const noexcept;

    template <
        typename T,
        typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
    T get_value(int = 0) const noexcept
    {
        T d = 0;
        if (decode(d) == EOF)
            return 0;
        return d;
    }

#ifdef __STDC_IEC_559__ // IEEE-754
    template <> double get_value(float) const noexcept
    {
        union
        {
            double f64;
            uint64_t u64;
        };
        assert(is_number());
        switch (m_type)
        {
        case decimal:
            if (std::sscanf(m_value.c_str(), "%lf", &f64) == EOF)
                return 0;
            return f64;
        case octal:
        case hexadecimal:
            if (decode<uint64_t>(u64) == EOF)
                return 0;
            u64 = from_le(u64);
            return f64;
        default:
            assert(false && "invalid m_type " && m_type);
            return 0;
        }
    }

    template <> float get_value(float) const noexcept
    {
        union
        {
            float f32;
            uint32_t u32;
        };
        assert(is_number());
        switch (m_type)
        {
        case decimal:
            if (std::sscanf(m_value.c_str(), "%f", &f32) == EOF)
                return 0;
            return f32;
        case octal:
        case hexadecimal:
            if (decode<uint32_t>(u32) == EOF)
                return 0;
            u32 = from_le(u32);
            return f32;
        default:
            assert(false && "invalid m_type " && m_type);
            return 0;
        }
    }
#endif

    constexpr bool is_string() const noexcept { return m_type == string; }

    constexpr bool is_number() const noexcept
    {
        return m_type == decimal || m_type == octal || m_type == hexadecimal;
    }

private:
    template <
        typename T,
        typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
    constexpr int decode(T &d) const noexcept;

    template <
        typename T,
        typename = typename std::enable_if<std::is_integral<T>::value, T>::type>
    static std::string encode(type type, T &d) noexcept
    {
        std::ostringstream buffer;
        switch (type)
        {
        case decimal:
            buffer << std::dec;
            break;
        case octal:
            buffer << std::oct << "0";
            break;
        case hexadecimal:
            buffer << std::hex << "0x";
            break;
        default:
            assert(false && "invalid type " && type);
            break;
        }
        buffer << d;
        return buffer.str();
    }

    template <
        typename T,
        typename = typename std::enable_if<std::is_unsigned<T>::value, T>::type>
    static T from_le(T n) noexcept
    {
        const uint8_t *const np = (uint8_t *)&n;
        T value                 = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            value |= (T)(np[i]) << (8 * i);
        return value;
    }

    template <
        typename T,
        typename = typename std::enable_if<std::is_unsigned<T>::value, T>::type>
    static T to_le(T n) noexcept
    {
        T value = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            *(uint8_t *)&value = (uint8_t)((&n >> (8 * i)) & 0xFF);
        return value;
    }

    template <> int decode(uint64_t &d) const noexcept
    {
        int readed = EOF;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNu64, &d);
            break;
        case octal:
            readed = std::sscanf(m_value.c_str() + 1, "%" SCNo64, &d);
            break;
        case hexadecimal:
            readed = std::sscanf(m_value.c_str() + 2, "%" SCNx64, &d);
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(uint32_t &d) const noexcept
    {
        int readed = EOF;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNu32, &d);
            break;
        case octal:
            readed = std::sscanf(m_value.c_str() + 1, "%" SCNo32, &d);
            break;
        case hexadecimal:
            readed = std::sscanf(m_value.c_str() + 2, "%" SCNx32, &d);
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(uint16_t &d) const noexcept
    {
        int readed = EOF;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNu16, &d);
            break;
        case octal:
            readed = std::sscanf(m_value.c_str() + 1, "%" SCNo16, &d);
            break;
        case hexadecimal:
            readed = std::sscanf(m_value.c_str() + 2, "%" SCNx16, &d);
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(uint8_t &d) const noexcept
    {
        int readed = EOF;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNu8, &d);
            break;
        case octal:
            readed = std::sscanf(m_value.c_str() + 1, "%" SCNo8, &d);
            break;
        case hexadecimal:
            readed = std::sscanf(m_value.c_str() + 2, "%" SCNx8, &d);
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(int64_t &d) const noexcept
    {
        int readed  = EOF;
        uint64_t d2 = 0;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNi64, &d);
            break;
        case octal:
        case hexadecimal:
            readed = decode(d2);
            d      = *(int64_t *)&d2;
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(int32_t &d) const noexcept
    {
        int readed  = EOF;
        uint32_t d2 = 0;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNi32, &d);
            break;
        case octal:
        case hexadecimal:
            readed = decode(d2);
            d      = *(int32_t *)&d2;
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(int16_t &d) const noexcept
    {
        int readed  = EOF;
        uint16_t d2 = 0;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNi16, &d);
            break;
        case octal:
        case hexadecimal:
            readed = decode(d2);
            d      = *(int16_t *)&d2;
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    template <> int decode(int8_t &d) const noexcept
    {
        int readed  = EOF;
        uint64_t d2 = 0;
        switch (m_type)
        {
        case decimal:
            readed = std::sscanf(m_value.c_str(), "%" SCNi8, &d);
            break;
        case octal:
        case hexadecimal:
            readed = decode(d2);
            d      = *(int8_t *)&d2;
            break;
        default:
            assert(false && "invalid m_type " && m_type);
            break;
        }
        return readed;
    }

    type m_type;
    std::string m_value;
};

class node
{
public:
    template <typename T> struct node_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = value_type *;
        using reference         = value_type &;

        node_iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }

        node_iterator &operator++()
        {
            if (m_ptr != nullptr)
                m_ptr = m_ptr->m_next;
            return *this;
        }

        node_iterator operator++(int)
        {
            node_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend bool operator==(const node_iterator &a, const node_iterator &b)
        {
            return a.m_ptr == b.m_ptr;
        }

        friend bool operator!=(const node_iterator &a, const node_iterator &b)
        {
            return a.m_ptr != b.m_ptr;
        }

    private:
        pointer m_ptr;
    };
    using iterator       = node_iterator<node>;
    using const_iterator = node_iterator<const node>;

    node()
        : m_parent(nullptr), m_children(nullptr), m_next(nullptr), m_lineNo(-1)
    {
    }

    node(node *parent,
         const std::string &name,
         std::initializer_list<value> content = {},
         node *next                           = nullptr,
         size_t lineNo                        = -1)
        : m_name(name), m_content(content), m_parent(parent),
          m_children(nullptr), m_next(next), m_lineNo(lineNo)
    {
        if (m_parent)
        {
            if (m_parent->m_children)
            {
                m_next = m_parent->m_last_children->m_next;
                m_parent->m_last_children->m_next = this;
                m_parent->m_last_children         = this;
            }
            else
            {
                m_parent->m_children      = this;
                m_parent->m_last_children = this;
            }
        }
    }

    node(const node &node)
        : m_parent(nullptr), m_next(nullptr), m_lineNo(node.m_lineNo)
    {
        copy(node);
    }

    ~node() { free(); }

    node &operator=(const node &node)
    {
        if (&node != this)
        {
            free();
            copy(node);
        }
        return *this;
    }

    // creation methods
    node(const std::string &name,
         std::initializer_list<value> content = {},
         int lineNo                           = -1)
        : m_name(name), m_content(content), m_parent(nullptr),
          m_children(nullptr), m_last_children(nullptr), m_next(nullptr),
          m_lineNo(lineNo)
    {
    }
    void add_child(node &child)
    {
        if (m_children == nullptr)
        {
            m_children      = &child;
            m_last_children = &child;
        }
        else
        {
            m_last_children->m_next = &child;
            m_last_children         = &child;
        }
        child.m_next   = nullptr;
        child.m_parent = this;
    }

    bool insert_child(node &child, node &following_node)
    {
        if (following_node.m_parent != this)
            return false;

        if (m_children == &following_node)
        {
            child.m_next = m_children;
            m_children   = &child;
        }
        else
        {
            node *ch = m_children;
            while (ch && ch->m_next != &following_node)
                ch = ch->m_next;
            if (!ch)
                return false;
            child.m_next = &following_node;
            ch->m_next   = &child;
        }
        child.m_parent = this;
        return true;
    }

    bool insert_child_after(node &child, node &preceding_node)
    {
        if (preceding_node.m_parent != this)
            return false;

        child.m_next          = preceding_node.m_next;
        preceding_node.m_next = &child;
        child.m_parent        = this;
        if (&preceding_node == m_last_children)
            m_last_children = &child;
        return true;
    }

    bool remove_child(node &child)
    {
        if (child.m_parent != this)
            return false;
        if (m_children == &child)
        {
            if (m_children != m_last_children)
                m_children = child.m_next;
            else
                m_last_children = m_children = nullptr;
            goto remove;
        }
        {
            node *ch = m_children;
            while (ch->m_next)
            {
                if (ch->m_next == &child)
                {
                    ch->m_next = child.m_next;
                    if (&child == m_last_children)
                        m_last_children = ch;
                    goto remove;
                }
                ch = ch->m_next;
            }
        }
        return false;
    remove:
        child.m_parent = nullptr;
        child.m_next   = nullptr;
        return true;
    }

    // access methods
    std::string &name() { return m_name; }
    const std::string &name() const { return m_name; }

    std::vector<value> &content() { return m_content; }
    const std::vector<value> &content() const { return m_content; }

    node *&parent() { return m_parent; }
    const node *parent() const { return m_parent; }

    int depth(node *grand_parent = nullptr) const
    {
        const node *n = this;
        int ret       = -1;
        do
        {
            ret++;
            n = n->parent();
            if (n == grand_parent)
                return ret;
        } while (n);
        return EOF;
    }

    iterator begin() { return iterator(m_children); }
    iterator end() { return iterator(nullptr); }

    const_iterator begin() const { return const_iterator(m_children); }
    const_iterator end() const { return const_iterator(nullptr); }

    const_iterator cbegin() const { return const_iterator(m_children); }
    const_iterator cend() const { return const_iterator(nullptr); }

    bool at(const std::string &node_name, iterator *value)
    {
        for (auto &i : *this)
        {
            if (i.name() == node_name)
            {
                if (value)
                    *value = iterator(&i);
                return true;
            }
        }
        return false;
    }

    bool at(const std::string &node_name, const_iterator *value) const
    {
        for (const auto &i : *this)
        {
            if (i.name() == node_name)
            {
                if (value)
                    *value = const_iterator(&i);
                return true;
            }
        }
        return false;
    }

    iterator at(const std::string &node_name) noexcept
    {
        iterator it = end();
        at(node_name, &it);
        return it;
    }

    const_iterator at(const std::string &node_name) const noexcept
    {
        const_iterator it = end();
        at(node_name, &it);
        return it;
    }

    bool has(const std::string &node_name) const noexcept
    {
        return at(node_name, nullptr);
    }

    iterator operator[](const std::string &node_name) { return at(node_name); }
    const_iterator operator[](const std::string &node_name) const
    {
        return at(node_name);
    }

    int lineNo() const { return m_lineNo; }

private:
    std::string m_name;
    std::vector<value> m_content;

    node *m_parent, *m_children, *m_last_children, *m_next;
    size_t m_lineNo;

    void free()
    {
        node *c, *c2;
        for (c = m_children; c; c = c2)
        {
            c2 = c->m_next;
            delete c;
        }
    }

    void copy(const node &n)
    {
        m_name          = n.m_name;
        m_content       = n.m_content;
        m_lineNo        = n.m_lineNo;
        m_children      = nullptr;
        m_last_children = nullptr;

        for (auto &c : n)
            add_child(*new node(c));
    }
};

class document
{
public:
    /**
     * @brief Default constructor
     */
    document() : m_root(nullptr) {}

    /**
     * @brief Loads the given filename
     */
    document(const std::string &filename) : m_root(nullptr)
    {
        if (!load_file(filename))
            delete m_root;
    }

    /**
     * @brief Loads the given SNOT document from given stream.
     */
    document(std::basic_istream<char> &stream) : m_root(nullptr)
    {
        if (!load_stream(stream))
            delete m_root;
    }

    /**
     * @brief Copy costructor
     *
     * Deep copies all the SNOT tree of the given document
     */
    document(const document &doc) : m_root(nullptr) { copy(doc); }

    /**
     * @brief Frees the document root node
     */
    ~document() { delete m_root; }

    /**
     * @brief Deep copies the document
     */
    document &operator=(const document &doc)
    {
        delete m_root;
        copy(doc);
        return *this;
    }

    /**
     * @brief Parses filename as an SNOT document file and loads its data
     *
     * @param filename Filename for SNOT document
     * @return Returns true if successful, otherwise false
     */
    bool load_file(const std::string &filename, bool ignore_fail = false)
    {
        std::ifstream file;
        file.open(filename);
        if (!file)
            return false;
        return load_stream(file, ignore_fail);
    }

    /**
     * @brief Parses the contents as SNOT document and loads its data
     *
     * @param contents String that contains a SNOT document data
     * @return Returns true if successful, otherwise false
     */
    bool load_string(const char *contents, bool ignore_fail = false)
    {
        std::stringstream ss(contents);
        return load_stream(ss, ignore_fail);
    }

    /**
     * @brief Parses the contents as SNOT document and loads its data
     *
     * @param contents String that contains a SNOT document data
     * @return Returns true if successful, otherwise false
     */
    bool load_string(const std::string &contents, bool ignore_fail = false)
    {
        std::stringstream ss(contents);
        return load_stream(ss, ignore_fail);
    }

    /**
     * @brief Parses the data of a given stream as a SNOT document and loads its
     * data
     *
     * @param stream Stream that contains a SNOT document data
     * @return Returns true if successful, otherwise false
     */
    bool load_stream(std::basic_istream<char> &stream, bool ignore_fail = false)
    {
        SNOT_PARSER *parser;
        parser_userdata userdata;
        SNOT_CALLBACKS cx;

        cx.alloc         = malloc;
        cx.free          = free;
        cx.grow          = grow;
        cx.start_section = start_section;
        cx.end_section   = end_section;
        cx.string        = string;
        cx.number        = number;

        node *const root = new node("root");

        userdata.current = root;
        userdata.fail    = false;

        parser = snot_create(cx, &userdata);

        std::basic_istream<char>::int_type c;
        while (c = stream.get(),
               c != std::basic_istream<char>::traits_type::eof())
        {
            snot_parse(parser, c);
        }

        size_t columnNo = 0;
        while (!stream.eof() && !stream.bad() && !stream.fail())
        {
            const uint32_t c = decode_utf8(stream);
            columnNo++;
            if (c == '\n')
            {
                userdata.lineNo++;
                columnNo = 0;
            }

            // fill the parser each with character at a time
            const SNOT_RESULT result = snot_parse(parser, c);
            switch (result)
            {
            case SNOT_OK:
                break;
            case SNOT_ERROR_NO_MEMORY:
            case SNOT_ERROR_INVALID_CHARACTER:
            case SNOT_ERROR_PARTIAL:
            case SNOT_ERROR_TOKEN_TYPE_UNDEFINED:
                fprintf(stderr,
                        "%zu:%zu: error at %c(code: %d)\n",
                        userdata.lineNo,
                        columnNo,
                        c,
                        result);
                break;
            default:
                fprintf(stderr,
                        "%zu:%zu: unknown error at %c(code: %d)\n",
                        userdata.lineNo,
                        columnNo,
                        c,
                        result);
                break;
            }
        }

        snot_end(parser);
        snot_free(parser);

        if (!ignore_fail && userdata.fail)
            delete root;
        else
            m_root = root;

        return !userdata.fail;
    }

    /**
     * @brief Saves a SNOT document to a file
     *
     * @param filename Filename to save
     * @param indented if enabled, the file will be indented
     * @return Returns true if successful, otherwise false
     */
    bool save_file(const std::string &filename, bool indented = false)
    {
        std::ofstream file;
        file.open(filename);
        return save_stream(file, indented);
    }

    /**
     * @brief Saves a SNOT document to a stream
     *
     * @param stream Stream to save
     * @param indented if enabled, the file will be indented
     * @return Returns true if successful, otherwise false
     */
    bool save_stream(std::basic_ostream<char> &stream, bool indented = false)
    {
        if (!ok())
            return false;

        const node &n = *m_root;
        serialize_node(stream, indented, 0, n, false, false);
        return true;
    }

    /**
     * @brief Gets reference to root node pointer
     */
    node *&root() { return m_root; }

    /**
     * @brief Gets root node pointer
     */
    node *root() const { return m_root; }

    /**
     * @brief Checks if document has been loaded succesfully
     *
     * @return Returns true if successful, otherwise false
     */
    constexpr bool ok() const { return root() != nullptr; }

private:
    node *m_root;

    static bool serialize_value(std::basic_ostream<char> &stream,
                                bool indented,
                                const value &n,
                                bool need_separator)
    {
        const std::string &str = n;
        const bool has_space =
            str.find_first_of(" ,;.()\\") != std::string::npos;
        if (has_space)
        {
            if (indented)
                stream << " ";
            stream << "\"" << str << "\"";
            need_separator = false;
        }
        else
        {
            if (need_separator)
                stream << " ";
            stream << str;
            need_separator = true;
        }
        return need_separator;
    }

    static bool serialize_content(std::basic_ostream<char> &stream,
                                  bool indented,
                                  const node &n,
                                  bool need_separator)
    {
        bool first = true;
        for (auto const &f : n.content())
        {
            const std::string &value = f;

            if (first)
                first = false;
            else
            {
                stream << ",";
                need_separator = false;
            }

            need_separator =
                serialize_value(stream, indented, f, need_separator);
        }
        return need_separator;
    }

    static int serialize_node(std::basic_ostream<char> &stream,
                              bool indented,
                              int ident_level,
                              const node &n,
                              bool need_separator,
                              bool show_name = true)
    {
        int depth = 0;
        const std::string sp(ident_level, ' ');

        if (indented)
        {
            stream << sp;
            need_separator = false;
        }

        if (show_name)
        {
            if (indented)
                ident_level += 2;
            need_separator =
                serialize_value(stream, indented, n.name(), need_separator);
            depth++;
        }

        if (n.begin() != n.end() && indented) // multiple lines
        {
            if (show_name)
                stream << "\n";
            if (!n.content().empty() && indented)
                stream << sp << "  ";
        }

        need_separator = serialize_content(stream, indented, n, need_separator);
        if (!n.content().empty())
            depth++;

        if (n.begin() != n.end())
        {
            if (!n.content().empty())
            {
                stream << ",";
                depth--;
                if (indented)
                    stream << "\n";
            }

            int i_depth = 0;
            for (auto const &c : n)
            {
                while (i_depth)
                {
                    switch (i_depth)
                    {
                    case 1:
                        i_depth -= 1;
                        stream << ",";
                        need_separator = false;
                        break;
                    case 2:
                        i_depth -= 2;
                        stream << ";";
                        need_separator = false;
                        break;
                    default:
                        i_depth -= 3;
                        stream << ".";
                        need_separator = false;

                        break;
                    }
                    if (i_depth == 0 && indented)
                        stream << "\n";
                }
                i_depth += serialize_node(
                    stream, indented, ident_level, c, need_separator);
            }
            depth += i_depth;
        }
        return depth;
    }

    struct parser_userdata
    {
        node *current;
        bool fail;
        size_t lineNo;
    };

    static uint32_t decode_utf8_continuation(std::basic_istream<char> &stream,
                                             uint32_t result,
                                             int count)
    {
        for (; count; --count)
        {
            std::basic_istream<char>::int_type next = stream.get();
            if (next == std::char_traits<char>::eof() ||
                (next & 0xC0) != 0x80) // valid 10xx_xxxx
            {
                // not a valid continuation character
                stream.setstate(std::ios::badbit);
                return -1;
            }
            result = (result << 6) | (next & 0x3F);
        }
        return result;
    }

    static uint32_t decode_utf8(std::basic_istream<char> &stream)
    {
        std::basic_istream<char>::int_type next = stream.get();
        if (next == std::char_traits<char>::eof())
        {
            stream.setstate(std::ios::badbit);
            return -1;
        }

        std::basic_istream<char>::int_type mask;
        if (mask = 0x80, (next & mask) == 0x00) // 1-byte
            return next & ~0x80;
        else if (mask = 0xE0, (next & mask) == 0xC0) // 2-byte
            return decode_utf8_continuation(stream, next & ~mask, 1);
        else if (mask = 0xF0, (next & mask) == 0xE0) // 3-byte
            return decode_utf8_continuation(stream, next & ~mask, 2);
        else if (mask = 0xF8, (next & mask) == 0xF0) // 3-byte
            return decode_utf8_continuation(stream, next & ~mask, 3);

        stream.setstate(std::ios::badbit);
        return -1;
    }

    static void *grow(void *memory, size_t *size, size_t grow_size)
    {
        *size = *size + (((grow_size + 7) >> 3) << 3);
        return realloc(memory, *size);
    }

    static void start_section(SNOT_PARSER *parser, size_t id, void *userdata)
    {
        assert(parser);
        assert(userdata);
        parser_userdata *u = (parser_userdata *)userdata;

        if (u->fail)
            return;

        const char *name;
        snot_value(parser, id, &name, NULL);

        u->current = new node(u->current, std::string(name));
    }

    static void end_section(SNOT_PARSER *parser, size_t id, void *userdata)
    {
        assert(parser);
        assert(userdata);
        parser_userdata *u = (parser_userdata *)userdata;

        if (u->fail)
            return;

        const char *name;
        snot_value(parser, id, &name, NULL);

        assert(u->current);
        assert(u->current->name() == name);
        if (u->current == nullptr || u->current->name() != name)
            u->fail = true;
        else
            u->current = u->current->parent();
    }

    static void string(SNOT_PARSER *parser, size_t id, void *userdata)
    {
        assert(parser);
        assert(userdata);
        parser_userdata *u = (parser_userdata *)userdata;

        if (u->fail)
            return;

        const char *str;
        snot_value(parser, id, &str, NULL);

        if (u->current == nullptr)
            u->fail = true;
        else
            u->current->content().push_back(value(str));
    }

    static void number(SNOT_PARSER *parser, size_t id, void *userdata)
    {
        assert(parser);
        assert(userdata);
        parser_userdata *u = (parser_userdata *)userdata;

        if (u->fail)
            return;

        const char *str;
        snot_value(parser, id, &str, NULL);

        if (u->current == nullptr)
            u->fail = true;
        else
        {
            SNOT_NUMBER_TYPE numberType;
            snot_number_type(parser, id, &numberType);

            value::type type;
            switch (numberType)
            {
            case SNOT_DEC_NUMBER:
                type = value::decimal;
                break;
            case SNOT_OCT_NUMBER:
                type = value::octal;
                break;
            case SNOT_HEX_NUMBER:
                type = value::hexadecimal;
                break;
            default:
                u->fail = true;
                return;
            }
            u->current->content().push_back(value(str, type));
        }
    }

    void copy(const document &doc)
    {
        if (doc.m_root)
            m_root = new node(*doc.m_root);
        else
            m_root = nullptr;
    }
};
} // namespace snot
