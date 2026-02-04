#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

static uint32_t XpacHash(const std::string& s) {
    uint32_t h = 0;
    for (std::size_t i = s.size(); i-- > 0;) {
        h = static_cast<uint32_t>(static_cast<unsigned char>(s[i]) + 131u * h);
    }
    return h;
}

static std::string ToLower(std::string s){
    for(char& c: s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

int main(){
    const std::string mapping = "F:/SSR_Dev/SSR_Unity_Dev/SeEditor/Resources/MAPPING.GC";
    std::ifstream in(mapping);
    if(!in){ std::cerr << "Failed to read mapping\n"; return 1; }
    std::vector<std::string> lines;
    std::string line;
    while(std::getline(in,line)){
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.rfind("1254590908:",0)==0) continue;
        lines.push_back(line);
    }
    std::unordered_set<std::string> existing;
    existing.reserve(lines.size()*2);
    for(auto const& l: lines){
        auto c = l.find(':');
        auto s = l.find(';', c==std::string::npos?0:c+1);
        if(c!=std::string::npos && s!=std::string::npos){
            std::string h = l.substr(0,c);
            std::string v = l.substr(c+1, s-c-1);
            existing.insert(h + "|" + ToLower(v));
        }
    }

    const std::string base = "doomeggzone_dlc";
    const std::string extras[] = {"", "_4p", "_PCRT_SH_Data", "_4p_PCRT_SH_Data"};
    const std::string suffixes[] = {".zif", ".zig"};

    int added=0;
    for(auto const& e: extras){
        for(auto const& suf: suffixes){
            std::string path = ".\\Resource\\Tracks\\" + base + e + suf;
            std::string upper = path;
            for(char& c: upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            std::replace(upper.begin(), upper.end(), '/', '\\');
            uint32_t h = XpacHash(upper);
            std::string key = std::to_string(h) + "|" + ToLower(path);
            if(existing.find(key)==existing.end()){
                existing.insert(key);
                lines.push_back(std::to_string(h) + ":" + path + ";");
                ++added;
            }
        }
    }

    std::ofstream out(mapping, std::ios::binary|std::ios::trunc);
    for(auto const& l: lines) out << l << "\n";
    out << "1254590908:.\\__END__OF__FILE__;\n";
    std::cout << "Added " << added << " entries\n";
    return 0;
}
