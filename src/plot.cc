// Developed by Ivan Pogrebnyak, MSU

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <memory>

#include <TFile.h>
#include <TDirectory.h>
#include <TH1.h>
#include <TAxis.h>
// #include <TCanvas.h>
// #include <TLegend.h>
// #include <TLine.h>
// #include <TStyle.h>
// #include <TPaveStats.h>

#define TEST(var) \
  std::cout <<"\033[36m"<< #var <<"\033[0m"<< " = " << var << std::endl;

#include "program_options.hh"
#include "tkey.hh"
#include "group_map.hh"
#include "plot_regex.hh"
#include "shared_str.hh"

using std::cout;
using std::endl;
using std::cerr;
using ivanp::cat;
using ivanp::error;

std::vector<plot_regex> exprs;

class hist {
  const char* get_file_str() {
    auto* dir = h->GetDirectory();
    while (!dir->InheritsFrom(TFile::Class())) dir = dir->GetMotherDir();
    return dir->GetName();
  }
  std::string get_dirs_str() {
    std::string dirs;
    for ( auto* dir = h->GetDirectory();
          !dir->InheritsFrom(TFile::Class());
          dir = dir->GetMotherDir() )
    {
      if (dirs.size()) dirs += '/';
      dirs += dir->GetName();
    }
    return dirs;
  }

  std::string init_impl(flags::field field) {
    switch (field) {
      case flags::n: return h->GetName(); break;
      case flags::t: return h->GetTitle(); break;
      case flags::x: return h->GetXaxis()->GetTitle(); break;
      case flags::y: return h->GetYaxis()->GetTitle(); break;
      case flags::z: return h->GetZaxis()->GetTitle(); break;
      case flags::d: return get_dirs_str(); break;
      case flags::f: return get_file_str(); break;
      default: return { };
    }
  }
  inline shared_str init(flags::field field) {
    return make_shared_str(init_impl(field));
  }

public:
  TH1 *h;
  shared_str legend;
  hist(TH1* h): h(h) { }
  // hist(const hist&) = default;
  hist(hist&& o): h(o.h), legend(std::move(o.legend)) { o.h = nullptr; }

  inline TH1& operator*() noexcept { return *h; }
  inline TH1* operator->() noexcept { return h; }

  bool operator()(shared_str& group) {
    if (exprs.empty()) { group = init(flags::n); return true; }

    // temporary strings
    std::array<std::vector<shared_str>,flags::nfields> tmps;
    for (auto& vec : tmps) { vec.emplace_back(); }

    for (const plot_regex& expr : exprs) {
      auto& vec = tmps[expr.from];
      int index = expr.from_i;
      if (index<0) index += vec.size(); // make index positive
      if (index<0 || (unsigned(index))>vec.size()) // overflow check
        throw error("out of range field string version index");

      auto& str = vec[index];
      if (!str) { // initialize string
        if (expr.from == flags::g) {
          auto& name = tmps[flags::n].back();
          if (!name) str = name = init(flags::n); // default g to n
        } else str = init(expr.from);
      }

      cout << '[' << expr.from << "] " << *str << endl;

      // TODO: apply regex

    } // end expressions loop

    if (!(group == std::move(tmps[flags::g].back())))
      if (!(group == std::move(tmps[flags::n].back()))) // default g to n
        group = init(flags::n);

    return true;
  }
}; // end hist

group_map<
  hist, shared_str,
  deref_pred<std::hash<std::string>>,
  deref_pred<std::equal_to<std::string>>
> hist_map;

void loop(TDirectory* dir) { // LOOP
  for (TKey& key : get_keys(dir)) {
    const TClass* key_class = get_class(key);

    if (key_class->InheritsFrom(TH1::Class())) { // HIST

      hist _h(read_key<TH1>(key));
      shared_str group;

      if ( _h(group) ) { // add hist if it passes selection
        if (!group) {
          cerr << "group was not assigned" << endl;
          continue;
        }
        hist_map[group].emplace_back(std::move(_h));
      } else continue;

    } else if (key_class->InheritsFrom(TDirectory::Class())) { // DIR
      loop(read_key<TDirectory>(key));
    }
  }
}

#define BOOST_REGEX_URL \
  "http://www.boost.org/libs/regex/doc/html/boost_regex/"

int main(int argc, char* argv[]) {
  const char* ofname;
  std::vector<const char*> ifnames;
  std::vector<const char*> expr_args;

  try {
    using namespace ivanp::po;
    if (program_options()
      (ifnames,'i',"input files (.root)",req(),pos())
      (ofname,'o',"output file (.pdf)",req())
      (expr_args,'r',"regular expressions")
      .help_suffix(
        "Regex expression syntax:\n"
        BOOST_REGEX_URL "syntax/perl_syntax.html\n"
        "Regex captures syntax:\n"
        BOOST_REGEX_URL "captures.html\n"
        "Regex format string syntax:\n"
        BOOST_REGEX_URL "format/boost_format_syntax.html\n"
      )
      .parse(argc,argv,true)) return 0;

    if (!ivanp::ends_with(ofname,".pdf")) throw ivanp::error(
      "output file name must end in \".pdf\"");

    exprs.reserve(expr_args.size());
    for (const auto& expr : expr_args) exprs.emplace_back(expr);
  } catch (const std::exception& e) {
    cerr <<"\033[31m"<< e.what() <<"\033[0m"<< endl;
    return 1;
  }

  std::vector<std::unique_ptr<TFile>> ifiles;
  ifiles.reserve(ifnames.size());
  for (const char* name : ifnames) {
    ifiles.emplace_back(std::make_unique<TFile>(name));
    TFile* f = ifiles.back().get();
    if (f->IsZombie()) return 1;
    cout << "\033[34mInput file:\033[0m " << f->GetName() << endl;

    loop(f);
  }

  cout << "\nGroups:\n";
  for (const auto& g : hist_map) {
    cout << *g.first << '\n';
  }
  cout.flush();

  // auto str = make_shared_str("jets_N_incl");
  // auto out = exprs[0](str);
  //
  // if (!out) {
  //   cout << "Didn't match" << endl;
  // } else {
  //   cout << ( out==str ? "Unchanged: " : "Result: " ) << *out << endl;
  // }

}
