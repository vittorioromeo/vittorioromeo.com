#include <SSVUtils/Core/Core.hpp>
#include <SSVUtils/Json/Json.hpp>
#include <SSVUtils/TemplateSystem/TemplateSystem.hpp>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <vrm/core/strong_typedef.hpp>

inline constexpr bool verbose{false};

using namespace std::string_literals;

template <typename... Ts>
[[nodiscard]] auto& lo_verbose(Ts&&... xs)
{
    if constexpr(verbose)
    {
        return ssvu::lo(FWD(xs)...);
    }
    else
    {
        [[maybe_unused]] static auto ln = ssvu::loNull();
        return ln;
    }
}

using sz_t = std::size_t;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
VRM_CORE_STRONG_TYPEDEF(sz_t, aside_id);
VRM_CORE_STRONG_TYPEDEF(sz_t, page_id);
VRM_CORE_STRONG_TYPEDEF(sz_t, entry_id);
#pragma GCC diagnostic pop

namespace constant::folder::name
{
    const std::string pages{"_pages"};
    const std::string entries{"_entries"};
    const std::string asides{"_asides"};

    const std::string content{"content"};
    const std::string resources{"resources"};
    const std::string result{"result"};
    const std::string temp{"temp"};
} // namespace constant::folder::name

namespace constant::folder::path
{
    const std::string content{folder::name::content + "/"};
    const std::string pages{content + folder::name::pages + "/"};
    const std::string result{folder::name::result + "/"};
    const std::string temp{folder::name::temp + "/"};
    const std::string resources{"/" + folder::name::resources};
} // namespace constant::folder::path

namespace constant::file
{
    const std::string page_json{"_page.json"};
    const std::string main_menu_json{"_menu.json"};
} // namespace constant::file

namespace constant::url::path
{
    const std::string website{"https://vittorioromeo.com/"};
} // namespace constant::url::path

namespace utils
{
    [[nodiscard]] std::string get_pubdate_mo(const std::string& mo)
    {
        if(mo == "january") return "Jan";
        if(mo == "february") return "Feb";
        if(mo == "march") return "Mar";
        if(mo == "april") return "Apr";
        if(mo == "may") return "May";
        if(mo == "june") return "Jun";
        if(mo == "july") return "Jul";
        if(mo == "august") return "Aug";
        if(mo == "september") return "Sep";
        if(mo == "october") return "Oct";
        if(mo == "november") return "Nov";
        if(mo == "december") return "Dec";

        assert(false);
        return "Dec";
    }

    [[nodiscard]] std::string to_pubdate(const std::string& date)
    {
        int d, y;
        std::string month;

        std::istringstream iss;
        iss.str(date);

        iss >> d >> month >> y;

        return std::to_string(d) + " " + get_pubdate_mo(month) + " " +
               std::to_string(y) + " 00:00:00 GMT";
    }

    template <typename T>
    [[nodiscard]] std::size_t find_nth(const std::string& haystack, sz_t pos,
        const T& needle, sz_t nth) noexcept
    {
        auto found_pos = haystack.find(needle, pos);

        if(nth == 0 || std::string::npos == found_pos)
        {
            return found_pos;
        }

        return find_nth(haystack, found_pos + 1, needle, nth - 1);
    }

    template <typename T>
    void exec_cmd(T&& x)
    {
#ifndef WIN32
        std::string cmd(FWD(x));
#else
        const std::string cmd =
            "\"C:\\Program Files\\Git\\bin\\bash.exe\" -c '" + FWD(x) + "'";
#endif

        ssvu::lo("system") << cmd << '\n';
        system(cmd.c_str());
    }

    namespace impl
    {
        [[nodiscard]] std::string html_from_md(const ssvufs::Path& p)
        {
            static std::mutex mtx;
            static int tempIdx = 0;

            std::string tempName;
            std::string outName;

            {
                std::scoped_lock lock(mtx);
                ++tempIdx;

                tempName = "temp" + std::to_string(tempIdx) + ".html";
                outName = "x" + std::to_string(tempIdx) + ".md";
            }

            const auto& tempf = constant::folder::path::temp;

            {
                std::ostringstream oss;
#if 1
#ifndef WIN32
                oss << "mkdir -p " << tempf << " ; touch " << tempf << tempName
                    << " ; chmod 777 " << tempf << tempName << " ; "
                    << "/usr/local/bin/pp -en " << p << " > " << tempf
                    << outName << "; "
                    << "pandoc --mathjax --highlight-style=pygments " << tempf
                    << outName << " -o " << tempf << tempName;
#else
                oss << "mkdir -p " << tempf << " ; touch " << tempf << tempName
                    << " ; chmod 777 " << tempf << tempName << " ; "
                    << "/c/OHWorkspace/vittorioromeo.info/pp.exe -en " << p
                    << " > " << tempf << outName << "; "
                    << "/c/OHWorkspace/vittorioromeo.info/pandoc.exe --mathjax "
                       "--highlight-style=pygments "
                    << tempf << outName << " -o " << tempf << tempName;
#endif
#else
                oss << "mkdir -p " << tempf << " ; touch " << tempf << tempName
                    << " ; chmod 777 " << tempf << tempName << " ; "
                    << "/usr/local/bin/pp -en " << p << " > " << tempf
                    << outName << "; "
                    << "pandoc -s -f markdown+four_space_rule --mathjax "
                       "--highlight-style pygments "
                    << tempf << outName << " -o " << tempf << tempName;
#endif

                utils::exec_cmd(oss.str());
            }

            // TODO: nasty hack: find "resources/" and replace with
            // "/resources/" for `pp` diagram generation
            // TODO: consider special variable such as "$RESOURCES"?

            std::ifstream ifs{tempf + tempName};
            return ssvu::getReplacedAll(
                std::string{std::istreambuf_iterator<char>(ifs),
                    std::istreambuf_iterator<char>()},
                "=\"resources/", "=\"/resources/");
            ;
        }

        template <typename T>
        [[nodiscard]] std::string expand_kv_pair(
            const ssvufs::Path& working_directory, const T& p)
        {
            if(ssvu::endsWith(p, ".md"))
            {
                const ssvufs::Path md_path =
                    working_directory + ssvufs::Path{p};

                return html_from_md(md_path);
            }

            return p;
        }
    } // namespace impl

    [[nodiscard]] ssvu::TemplateSystem::Dictionary expand_to_dictionary(
        const ssvufs::Path& working_directory, const ssvj::Val& mVal)
    {
        using namespace ssvj;

        ssvu::TemplateSystem::Dictionary result;

        for(const auto& p : mVal.forObj())
        {
            if(p.value.is<Str>())
            {
                const Str& o = p.value.as<Str>();
                result[p.key] = impl::expand_kv_pair(working_directory, o);
            }
            else if(p.value.is<std::vector<Val>>())
            {
                for(auto x : p.value.as<std::vector<Val>>())
                {
                    const ssvu::TemplateSystem::Dictionary inner =
                        expand_to_dictionary(working_directory, x);

                    result[p.key] += inner;
                }
            }
        }

        return result;
    }

    void write_to_file(const ssvufs::Path& p, const std::string& s)
    {
        const ssvufs::Path p_parent = p.getParent();
        if(!p_parent.exists<ssvufs::Type::Folder>())
        {
            exec_cmd("mkdir -p "s + p_parent.getStr());
        }

        assert(p_parent.exists<ssvufs::Type::Folder>());

        std::ofstream o{p};
        o << s;
        o.flush();
    }

    template <typename TF>
    void segmented_for(sz_t split_count, sz_t per_split, sz_t total, TF&& f)
    {
        // TODO:
        if(split_count <= 0)
        {
            return;
        }

        for(sz_t i_split = 0; i_split < split_count - 1; ++i_split)
        {
            auto i_begin = i_split * per_split;
            auto i_end = (i_split + 1) * per_split;
            f(i_split, i_begin, i_end);
        }

        {
            auto i_split = split_count - 1;
            auto i_begin = i_split * per_split;
            auto i_end = total;
            f(i_split, i_begin, i_end);
        }
    }

    [[nodiscard]] std::string expand_to_str(
        ssvu::TemplateSystem::Dictionary& d, const std::string& p)
    {
        static std::map<std::string, std::string> memoized_templates;

        if(memoized_templates.count(p) == 0)
        {
            memoized_templates[p] = ssvufs::Path{p}.getContentsAsStr();
        }

        return d.getExpanded(memoized_templates[p],
            ssvu::TemplateSystem::Settings::EraseUnexisting);
    }

    template <typename T>
    [[nodiscard]] std::string result_to_website(const T& x)
    {
        return ssvu::getReplaced(
            x, constant::folder::path::result, constant::url::path::website);
    }
} // namespace utils

namespace archetype
{
    struct main_menu_entry
    {
        std::string _label;
        std::string _href;
    };

    struct main_menu
    {
        std::vector<main_menu_entry> _menu_entries;
    };

    namespace impl
    {
        struct element
        {
            // TODO: probably good place to use `string_view`: store full path
            // in an `std::string` and slices in `string_view`

            ssvufs::Path _template_path;
            ssvu::TemplateSystem::Dictionary _expand;
            ssvufs::Path _output_path;
            page_id _parent_page;
        };
    } // namespace impl

    struct aside : public impl::element
    {
    };

    struct entry : public impl::element
    {
        std::optional<std::string> _link_name;
        std::vector<std::string> _tags;
    };

    struct page
    {
        std::shared_ptr<std::mutex> _mtx = std::make_shared<std::mutex>();

        std::string _name;
        ssvufs::Path _path;
        std::string _full_name;
        ssvufs::Path _output_path;
        std::optional<sz_t> _subpaging;
        std::optional<std::string> _rss;
        std::vector<std::pair<int, entry_id>> _entries;
        std::vector<aside_id> _asides;
    };
} // namespace archetype

namespace structure
{
    namespace impl
    {
        template <typename TID, typename TArchetype>
        class mapping
        {
        private:
            mutable std::mutex _mtx;
            std::map<TID, TArchetype> _archetypes;
            TID _next_id{0};

        public:
            template <typename TF>
            void create(TF&& f)
            {
                TID id;
                TArchetype* slot;

                {
                    std::scoped_lock lock{_mtx};

                    id = _next_id;
                    slot = &_archetypes[_next_id];

                    ++_next_id;
                }

                f(id, *slot);
            }

            [[nodiscard]] const TArchetype& get(TID id) const
            {
                std::scoped_lock lock{_mtx};

                return _archetypes.at(id);
            }

            template <typename TF>
            void for_all(TF&& f)
            {
                std::scoped_lock lock{_mtx};

                for(auto& [k, v] : _archetypes)
                {
                    f(k, v);
                }
            }

            template <typename TF>
            void for_all(TF&& f) const
            {
                std::scoped_lock lock{_mtx};

                for(const auto& [k, v] : _archetypes)
                {
                    f(k, v);
                }
            }
        };
    } // namespace impl

    using aside_mapping = impl::mapping<aside_id, archetype::aside>;
    using entry_mapping = impl::mapping<entry_id, archetype::entry>;
    using page_mapping = impl::mapping<page_id, archetype::page>;
} // namespace structure

using namespace ssvu::FileSystem;
using namespace ssvu::TemplateSystem;
using namespace ssvu::Json;

template <typename TF>
void for_all_page_json_files(TF&& f)
{
    lo_verbose("for_all_page_json_files")
        << "scanning directory '" << constant::folder::path::pages << "'\n";

    const std::vector<Path> page_json_paths{
        getScan<Mode::Recurse, Type::File, Pick::ByName>(
            constant::folder::path::pages, constant::file::page_json)};

    for(const ssvufs::Path& p : page_json_paths)
    {
        const ssvufs::Path& path = p;
        lo_verbose("for_all_page_json_files") << "path '" << path << "'\n";

        // Name of the folder containing "_page.json".
        const std::string& name = p.getParent().getFolderName();
        lo_verbose("for_all_page_json_files") << "name '" << name << "'\n";

        // Remove "_pages" and right-trim until first "/".
        const std::string& full_name =
            ssvu::getTrimR(ssvu::getReplaced(p.getParent().getStr(),
                               constant::folder::path::pages, ""),
                [](char c) { return c == '/'; });

        lo_verbose("for_all_page_json_files")
            << "full_name '" << full_name << "'\n";

        const ssvj::Val& json_val = ssvj::fromFile(path);
        f(path, name, full_name, json_val);
    }
}

// TODO: patreon button in template

namespace impl
{
    template <typename TF>
    void for_all_page_element_files(const std::string& element_folder_name,
        const ssvufs::Path& page_path, TF&& f)
    {
        const ssvufs::Path elements_path =
            page_path.getParent() + element_folder_name;

        if(!elements_path.exists<ssvufs::Type::Folder>())
        {
            return;
        }

        lo_verbose("for_all_page_element_files")
            << "scanning directory '" << elements_path << "'\n";

        const std::vector<Path> json_files{
            getScan<Mode::Recurse, Type::File, Pick::ByExt>(
                elements_path, ".json")};

        for(const ssvufs::Path& path : json_files)
        {
            // Filename without extension.
            const auto& name =
                ssvu::getReplaced(path.getFileName(), ".json", "");

            // Remove parent elements folder and `.json` extension.
            const auto& full_name = ssvu::getReplaced(
                ssvu::getReplaced(path.getStr(), elements_path, ""), ".json",
                "");

            // Read json contents.
            const ssvj::Val& json_val = ssvj::fromFile(path);
            f(path, name, full_name, json_val);
        }
    }

    template <typename TF>
    void for_all_elements(const std::string& element_folder_name,
        const ssvufs::Path& page_path, TF&& f)
    {
        for_all_page_element_files(element_folder_name, page_path,
            [&f](auto path, auto name, auto full_name, const Val& element_json)
            {
                // Force link/output name of a group of entries.
                if(element_json.has("link_name"))
                {
                    const Str link_name = element_json["link_name"].as<Str>();
                    ssvu::replace(full_name, name, link_name);
                    name = link_name;
                }

                for(const Val& element : element_json["elements"].forArr())
                {
                    f(path, name, full_name, element);
                }
            });
    }
} // namespace impl

template <typename TF>
void for_all_entry_json_files(const ssvufs::Path& page_path, TF&& f)
{
    impl::for_all_page_element_files(
        constant::folder::name::entries, page_path, FWD(f));
}

template <typename TF>
void for_all_aside_json_files(const ssvufs::Path& page_path, TF&& f)
{
    impl::for_all_page_element_files(
        constant::folder::name::asides, page_path, FWD(f));
}

template <typename TF>
void for_all_entries(const ssvufs::Path& page_path, TF&& f)
{
    impl::for_all_elements(constant::folder::name::entries, page_path, FWD(f));
}

template <typename TF>
void for_all_asides(const ssvufs::Path& page_path, TF&& f)
{
    impl::for_all_elements(constant::folder::name::asides, page_path, FWD(f));
}

[[nodiscard]] std::string escape_xml(const std::string& x)
{
    std::string s;
    s.reserve(x.size() * 1.4f);

    for(std::size_t i = 0; i != x.size(); ++i)
    {
        switch(x[i])
        {
            case '&': s.append("&amp;"); break;
            case '\"': s.append("&quot;"); break;
            case '\'': s.append("&apos;"); break;
            case '<': s.append("&lt;"); break;
            case '>': s.append("&gt;"); break;
            case '`': break;
            default: s.append(&x[i], 1); break;
        }
    }

    return s;
}

struct context
{
    structure::aside_mapping _aside_mapping;
    structure::entry_mapping _entry_mapping;
    structure::page_mapping _page_mapping;

    // TODO: already expanded menu
    archetype::main_menu _main_menu;

    // structure::page_hierarchy _page_hierarchy;
};

struct subpage_expansion
{
    std::vector<std::string> _expanded_entries;
    std::vector<entry_id> _expanded_entry_ids;
    std::string _link;

    void write_rss_feed(
        bool first, const context& ctx, const archetype::page& ap) const
    {
        if(!ap._rss)
        {
            return;
        }

        const std::string feed_output_path = ssvu::getReplaced(
            first ? ap._output_path : Path{_link}, ".html", ".rss");

        Dictionary d;
        d["FeedLink"] = utils::result_to_website(feed_output_path);

        for(const auto& ei : _expanded_entry_ids)
        {
            const archetype::entry& ae = ctx._entry_mapping.get(ei);
            const auto& aee = ae._expand;
            if(!ae._link_name) continue;

            if(!aee.has("Title") || !aee.has("Date"))
            {
                continue;
            }

            Dictionary d_item;
            d_item["Title"] = escape_xml(aee["Title"].asStr());
            d_item["Link"] = utils::result_to_website(ae._output_path);
            d_item["Date"] = aee["Date"].asStr();
            d_item["Description"] = escape_xml(aee["Title"].asStr());
            d_item["PubDate"] = utils::to_pubdate(d_item["Date"].asStr());

            d["Items"] += d_item;
        }

        const std::string res =
            utils::expand_to_str(d, "templates/other/rss.tpl");

        utils::write_to_file(feed_output_path, res);
    }

    [[nodiscard]] std::string produce_expanded_main(const archetype::page& ap,
        const std::vector<subpage_expansion>& subpages,
        const std::vector<std::string>& expanded_asides) const
    {
        Dictionary d_main;

        // Add expanded entries.
        for(const std::string& e : _expanded_entries)
        {
            d_main["Entries"] += Dictionary{"Entry", e};
        }

        // Add expanded asides.
        for(const std::string& a : expanded_asides)
        {
            d_main["Asides"] += Dictionary{"Aside", a};
        }

        // Add pagination controls.
        if(ap._subpaging && subpages.size() > 1)
        {
            sz_t page_idx = 0;
            for(const auto& a : subpages)
            {
                Dictionary inner_dict;

                inner_dict["Subpage"] =
                    ssvu::getReplaced(a._link, "result/", "/");

                inner_dict["SubpageLabel"] =
                    a._link == _link ? "[" + std::to_string(page_idx++) + "]"
                                     : std::to_string(page_idx++);

                d_main["Subpages"] += inner_dict;
            }
        }

        return utils::expand_to_str(d_main, "templates/base/main.tpl");
    }

    [[nodiscard]] std::string produce_expanded_main_menu(
        const context& ctx) const
    {
        Dictionary d_mainmenu;

        for(const auto& mm_e : ctx._main_menu._menu_entries)
        {
            Dictionary d_button;
            d_button["Link"] = mm_e._href;
            d_button["Title"] = mm_e._label;

            d_mainmenu["MenuItems"] += d_button;
        }

        return utils::expand_to_str(d_mainmenu, "templates/base/mainMenu.tpl");
    }

    [[nodiscard]] std::string produce_expanded_page(
        const std::string& expanded_main,
        const std::string& expanded_main_menu) const
    {
        Dictionary d_page;
        d_page["Main"] = expanded_main;
        d_page["MainMenu"] = expanded_main_menu;
        d_page["ResourcesPath"] = constant::folder::path::resources;

        return utils::expand_to_str(d_page, "templates/page.tpl");
    }

    [[nodiscard]] std::string produce_result(bool first, const context& ctx,
        const archetype::page& ap,
        const std::vector<subpage_expansion>& subpages, const Path& output_path,
        const std::vector<std::string>& expanded_asides) const
    {
        (void)output_path;

        write_rss_feed(first, ctx, ap);

        auto expanded_main =
            produce_expanded_main(ap, subpages, expanded_asides);

        auto expanded_main_menu = produce_expanded_main_menu(ctx);

        return produce_expanded_page(expanded_main, expanded_main_menu);
    }
};

struct page_expansion
{
    std::vector<std::string> _expanded_asides;
    std::vector<subpage_expansion> _subpages;

    auto produce_result(
        const context& ctx, const archetype::page& ap, const Path& output_path)
    {
        assert(_subpages.size() > 0);

        // Generate asides
        for(const aside_id aid : ap._asides)
        {
            archetype::aside aa = ctx._aside_mapping.get(aid);

            _expanded_asides.emplace_back(
                utils::expand_to_str(aa._expand, aa._template_path));
        }

        // Set links
        subpage_expansion& first_subpage = _subpages[0];
        first_subpage._link = output_path.getStr();

        for(sz_t i = 1; i < _subpages.size(); ++i)
        {
            std::string adapted_op = output_path.getStr();

            ssvu::replace(
                adapted_op, ".html", "/" + std::to_string(i) + ".html");

            _subpages[i]._link = adapted_op;
        }

        // ---
        // Write to file
        const std::string first_subpage_html = first_subpage.produce_result(
            true, ctx, ap, _subpages, output_path, _expanded_asides);

        utils::write_to_file(output_path, first_subpage_html);

        for(sz_t i = 1; i < _subpages.size(); ++i)
        {
            const auto& s = _subpages[i];

            utils::write_to_file(
                s._link, s.produce_result(false, ctx, ap, _subpages,
                             Path{s._link}, _expanded_asides));
        }
    }
};

void process_page_entries(context& ctx, const Path& output_path,
    const Path& path, page_id pid, archetype::page& ap)
{
    int ordering = 0;
    std::vector<std::future<void>> todo;

    for_all_entries(path,
        [&ordering, &ctx, &output_path, &pid, &ap, &todo](
            auto e_path, auto e_name, auto e_full_name, const Val& e_contents)
        {
            ++ordering;

            auto f = [ordering, &ctx, output_path, pid, &ap, e_path, e_name,
                         e_full_name, e_contents]
            {
                ctx._entry_mapping.create(
                    [&](auto eid, auto& ae)
                    {
                        lo_verbose("Entry|parent_page") << pid << "\n";
                        lo_verbose("Entry|path")
                            << e_path << " [" << eid << "]\n";
                        lo_verbose("Entry|name")
                            << e_name << "/" << eid << "\n";
                        lo_verbose("Entry|full_name")
                            << e_full_name << "/" << eid << "\n";

                        auto e_template_path = e_contents["template"].as<Str>();
                        auto e_expand_data = e_contents["expand"].as<Val>();
                        auto wd = e_path.getParent();
                        auto dic =
                            utils::expand_to_dictionary(wd, e_expand_data);

                        auto e_output_path =
                            Path{ssvu::getReplaced(output_path, ".html", "")} +
                            "/" + e_full_name;

                        // Register entry.
                        {
                            std::scoped_lock lock(*ap._mtx);
                            ap._entries.emplace_back(ordering, eid);
                        }


                        if(e_contents.has("link_name"))
                        {
                            auto link_name = e_contents["link_name"].as<Str>();
                            ae._link_name = link_name;
                            e_output_path += "/" + link_name + ".html";
                        }
                        else
                        {
                            e_output_path +=
                                "/" + std::to_string(eid) + ".html";
                        }

                        if(e_contents.has("tags"))
                        {
                            for(const auto& t :
                                e_contents["tags"].forArrAs<Str>())
                            {
                                ae._tags.emplace_back(t);
                            }
                        }
                        else
                        {
                            ae._tags.emplace_back("untagged");
                        }

                        ae._template_path = e_template_path;
                        ae._expand = dic;
                        ae._output_path = e_output_path;
                        ae._parent_page = pid;

                        lo_verbose("Entry|output_path")
                            << e_output_path << "\n";
                        lo_verbose("Entry|template") << e_template_path << "\n";
                        // lo_verbose("Entry|expand") << e_expand_data <<
                        // "\n";
                        lo_verbose() << "\n";
                    });
            };

            todo.emplace_back(std::async(f));
        });
}

void process_page_asides(context& ctx, const Path& output_path,
    const Path& path, page_id pid, archetype::page& ap)
{
    for_all_asides(path,
        [&ctx, &pid, &ap, &output_path](
            auto a_path, auto a_name, auto a_full_name, const Val& a_contents)
        {
            ctx._aside_mapping.create(
                [&](auto aid, auto& aa)
                {
                    lo_verbose("Aside|parent_page") << pid << "\n";
                    lo_verbose("Aside|path") << a_path << "\n";
                    lo_verbose("Aside|name") << a_name << "\n";
                    lo_verbose("Aside|full_name") << a_full_name << "\n";

                    auto template_path = a_contents["template"].as<Str>();
                    auto a_expand_data = a_contents["expand"].as<Val>();

                    auto a_output_path =
                        Path{ssvu::getReplaced(output_path, ".html", "")} +
                        "/" + a_full_name;

                    auto wd = a_path.getParent();
                    auto dict = utils::expand_to_dictionary(wd, a_expand_data);

                    // Register aside.
                    {
                        std::scoped_lock lock(*ap._mtx);
                        ap._asides.emplace_back(aid);
                    }

                    aa._parent_page = pid;
                    aa._template_path = template_path;
                    aa._output_path = a_output_path;
                    aa._expand = dict;

                    // Increment unique aside id.
                    lo_verbose() << "\n";
                });
        });
}

void clean_and_recreate_result_folder()
{
    Path rp{constant::folder::path::result};
    assert(ssvu::endsWith(rp, "/"));

    if(!rp.exists<Type::Folder>())
    {
        ssvufs::createFolder(rp);
    }

    utils::exec_cmd("rm -R ./"s + rp.getStr() + "*");
    utils::exec_cmd("ln -s ../resources/ ./" + rp.getStr());
}

void load_main_menu_data(context& ctx)
{
    const Path main_menu_json_path{
        constant::folder::path::content + constant::file::main_menu_json};

    const ssvj::Val main_menu_json = ssvj::fromFile(main_menu_json_path);
    std::vector<archetype::main_menu_entry>& mm_entries =
        ctx._main_menu._menu_entries;

    for(const ssvu::Json::Impl::Val& mm_e : main_menu_json.forArr())
    {
        archetype::main_menu_entry& last_e = mm_entries.emplace_back();

        last_e._label = mm_e["label"].as<Str>();
        last_e._href = mm_e["href"].as<Str>();

        lo_verbose("MMEntry|label") << last_e._label << "\n";
        lo_verbose("MMEntry|href") << last_e._href << "\n\n";
    };
}

void load_page_data(context& ctx)
{
    std::vector<std::future<void>> todo;

    for_all_page_json_files(
        [&ctx, &todo](auto path, auto name, auto full_name, Val contents)
        {
            // Register page.
            ctx._page_mapping.create(
                [&](auto pid, auto& ap)
                {
                    auto output_path =
                        std::string{constant::folder::path::result} +
                        full_name + ".html";

                    // Log data about page.
                    lo_verbose("Page|id") << pid << "\n";
                    lo_verbose("Page|path") << path << "\n";
                    lo_verbose("Page|name") << name << "\n";
                    lo_verbose("Page|full_name") << full_name << "\n";
                    lo_verbose("Page|output_path") << output_path << "\n";

                    {
                        std::scoped_lock lock(*ap._mtx);

                        ap._name = name;
                        ap._path = path;
                        ap._full_name = full_name;
                        ap._output_path = output_path;
                    }

                    // Check for subpaging options.
                    if(contents.has("subpaging"))
                    {
                        auto eps = contents["subpaging"]["entries_per_subpage"]
                                       .as<IntU>();

                        {
                            std::scoped_lock lock(*ap._mtx);
                            ap._subpaging = eps;
                        }

                        lo_verbose("Page|eps") << eps << "\n";
                    }

                    // Check for RSS options.
                    if(contents.has("rss"))
                    {
                        {
                            std::scoped_lock lock(*ap._mtx);
                            ap._rss = contents["rss"]["output"].as<Str>();
                        }
                    }

                    todo.emplace_back(std::async(
                        [&ctx, output_path, path, pid, &ap]
                        {
                            process_page_entries(
                                ctx, output_path, path, pid, ap);

                            process_page_asides(
                                ctx, output_path, path, pid, ap);
                        }));
                });
        });
}

void build_tag_expansion(archetype::entry& ae)
{
    for(const auto& t : ae._tags)
    {
        Dictionary tag0;
        tag0["Link"] = "#";
        tag0["Label"] = t;
        ae._expand["Tags"] += tag0;
    }
}

void process_pages_permalink(
    const context& ctx, const archetype::page& ap, entry_id eid)
{
    // Create copy of page archetype for the single-article pages
    // and disable automatic pagination/RSS
    auto my_ap = ap;
    my_ap._subpaging = std::nullopt;
    my_ap._rss = std::nullopt;

    auto ae = ctx._entry_mapping.get(eid);
    if(!ae._link_name)
    {
        return;
    }


    page_expansion permalink_pe;
    permalink_pe._subpages.emplace_back();
    auto& subpage = permalink_pe._subpages.back();

    auto permalink_output_path = ae._output_path;
    auto canonical_permalink_url =
        utils::result_to_website(permalink_output_path.getStr());

    // Disqus
    {
        Dictionary disqus;
        disqus["PageUrl"] = canonical_permalink_url;
        disqus["PageId"] = ae._link_name.value();

        auto disqus_template =
            Path{"templates/other/disqus.tpl"}.getContentsAsStr();
        auto expanded_disqus =
            disqus.getExpanded(disqus_template, Settings::EraseUnexisting);

        ae._expand["CommentsBox"] = expanded_disqus;
    }



    build_tag_expansion(ae);
    auto e_expanded = utils::expand_to_str(ae._expand, ae._template_path);

    subpage._expanded_entries.emplace_back(e_expanded);
    permalink_pe.produce_result(ctx, my_ap, permalink_output_path);
}

void process_entries_ellipsis_and_permalink(archetype::entry& ae)
{
    if(!ae._link_name)
    {
        // No permalink.
        return;
    }

    auto atag_href =
        ssvu::getReplaced(ae._output_path, constant::folder::path::result, "");

    auto atag_link = "<a href='/" + atag_href + "'>";

    auto atag_link_styled = "<h2><a href='/" + atag_href + "'>";

    // Ellipse long text
    std::string old_text = ae._expand["Text"].asStr();
    auto second_paragraph = utils::find_nth(old_text, 0, "</p>", 2);

    if(second_paragraph != std::string::npos)
    {
        auto new_text = old_text.substr(0, second_paragraph + 4);
        new_text +=
            "<p style='text-align: right; font-style: italic; font-size: 85%;'> "s +
            atag_link + " ... read more </a></p></body></html>";

        ae._expand["Text"] = new_text;
    }

    ae._expand["PermalinkBegin"] = atag_link_styled;
    ae._expand["PermalinkEnd"] = "</a></h2>";
}

void process_pages(context& ctx)
{
    ctx._page_mapping.for_all(
        [&ctx](auto, archetype::page& ap)
        {
            std::sort(ap._entries.begin(), ap._entries.end(),
                [](const auto& e0, const auto& e1)
                { return e0.first < e1.first; });

            const auto& entry_ids = ap._entries;
            if(entry_ids.empty())
            {
                return;
            }

            page_expansion pe;

            // Expand permalinks (single-article pages)
            for(auto [order, eid] : entry_ids)
            {
                process_pages_permalink(ctx, ap, eid);
            }

            auto make_subpage = [&](auto i_begin, auto i_end)
            {
                pe._subpages.emplace_back();
                auto& subpage = pe._subpages.back();

                for(sz_t ei(i_begin); ei < i_end; ++ei)
                {
                    entry_id eid = entry_ids[ei].second;
                    auto ae = ctx._entry_mapping.get(eid);
                    process_entries_ellipsis_and_permalink(ae);

                    build_tag_expansion(ae);
                    auto e_expanded =
                        utils::expand_to_str(ae._expand, ae._template_path);

                    subpage._expanded_entry_ids.emplace_back(eid);
                    subpage._expanded_entries.emplace_back(e_expanded);
                }
            };

            // Create subpages
            if(ap._subpaging)
            {
                auto entries_per_subpage = ap._subpaging.value();
                auto subpage_count =
                    std::max(sz_t(1), entry_ids.size() / entries_per_subpage);

                utils::segmented_for(subpage_count, entries_per_subpage,
                    entry_ids.size(),
                    [&](auto, auto i_begin, auto i_end)
                    { make_subpage(i_begin, i_end); });
            }
            else
            {
                make_subpage(0, entry_ids.size());
            }

            pe.produce_result(ctx, ap, ap._output_path);
        });
}

int main()
{
    lo_verbose("main") << "cleaning and re-creating result folder\n";
    clean_and_recreate_result_folder();

    context ctx;

    lo_verbose("main") << "loading main menu data\n";
    load_main_menu_data(ctx);

    lo_verbose("main") << "loading page data\n";
    load_page_data(ctx);

    lo_verbose("main") << "processing pages\n";
    process_pages(ctx);

    lo_verbose("main") << "done\n";
    return 0;
}
