#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <regex>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace {

// Класс принимает лог, предварительно считанный в строку, регулярное
// выражение для поиска доменов и путей и количество top доменов и путей,
// которое необходимо вывести (Через конструктор, либо через метод Prepare).
// При вызове метода GetStats производит вычисления и возвращает отчёт в
// виде строки.
class DomainPathCounter
{
public:
    DomainPathCounter() { }

    explicit DomainPathCounter(const std::string &input, std::regex re,
                               size_t n_top) :
        input_(input), re_(re), n_top_(n_top) { }

    // Задаёт исходные данные для расчетов.
    void Prepare(const std::string &input, std::regex re, size_t n_top) {
        input_ = input;
        re_ = re;
        n_top_ = n_top;
        num_urls_ = 0;
        domains_.clear();
        paths_.clear();
    }

    // Производит вычисления, формирует и возвращает отчёт.
    std::string GetStats() {
        FindDomainsAndNames();
        auto top_domains = GetTop(domains_);
        auto top_paths = GetTop(paths_);

        std::stringstream ss;
        ss << "total urls " << num_urls_ << ", domains "
            << domains_.size() << ", paths " << paths_.size() << "\n";

        ss << "\ntop domains\n";
        for (auto &elem : top_domains) {
            ss << elem.second << " " << elem.first << "\n";
        }

        ss << "\ntop paths\n";
        for (auto &elem : top_paths) {
            ss << elem.second << " " << elem.first << "\n";
        }

        return ss.str();
    }

private:
    using Count = size_t;
    using CountElem = std::pair<std::string, Count>;
    // В качестве структуры данных для подсчёта количества встречаний доменов
    // и путей используется хеш таблица, где ключом является домен/путь, а
    // значением счётчик количества встречаний.
    using CountBuffer = std::unordered_map<std::string, Count>;

    std::string input_;
    std::regex re_;
    size_t n_top_ = 0;
    CountBuffer domains_;
    CountBuffer paths_;
    size_t num_urls_ = 0;

    // Находит в логе все домены и пути и подсчитывает их количества.
    void FindDomainsAndNames() {
        std::string line;
        std::stringstream ss(input_);
        while (ss) {
            std::getline(ss, line);
            std::sregex_iterator next(line.begin(), line.end(), re_);
            std::sregex_iterator end;
            while (next != end) {
                auto match = *next;
                auto domain = match.format("$2");
                auto path = match.format("$3");
                // Если после домена ничего не было (пробел или EOL), то
                // берется путь по умолчанию.
                if (path.empty() || path == " ") {
                    path = "/";
                }

                domains_[domain]++;
                paths_[path]++;
                ++next;
                ++num_urls_;
            }
        }
    }

    // Возвращает отсортированный массив из максимум n_top_ (может быть меньше,
    // если данных недостаточно) элементов из buffer с наибольшими значениями
    // Count.
    // Реализовано путём реорганизации хеш таблицы в структуру binary heap и
    // извлечения из неё нужного количества элементов.
    std::vector<CountElem> GetTop(const CountBuffer& buffer) {
        auto cmp = [](const CountElem& in1, const CountElem& in2) {
                if (in1.second == in2.second) {
                    return in1.first.compare(in2.first) > 0 ? true : false;
                }
                else {
                    return in1.second < in2.second;
                }
            };

        std::vector<CountElem> heap;
        for (auto& elem : buffer) {
            heap.push_back(elem);
        }

        std::make_heap(heap.begin(), heap.end(), cmp);

        std::vector<CountElem> top;
        size_t num_top = std::min(n_top_, heap.size());
        for (size_t i = 0; i < num_top; ++i) {
            top.push_back(heap.front());
            std::pop_heap(heap.begin(),heap.end(), cmp);
            heap.pop_back();
        }

        return top;
    }
};

} // namespace

int main(int argc, char **argv)
{
    if (!(argc == 3 || argc == 5)) {
        std::cout << "usage: " << argv[0]
            << " [-n NNN] <input_file> <output_filename>\n";
        return -1;
    }

    int n = 1;
    const char* in_filename = nullptr;
    const char* out_filename = nullptr;

    if (argc == 5) {
        if (std::string(argv[1]) != "-n") {
            std::cout << "unrecognized parameter: " << argv[1] << "\n";
            return -1;
        }

        try {
            size_t pos = 0;
            n = std::stoi(argv[2], &pos);
            if (pos != std::string(argv[2]).size() || n <= 0) {
                throw std::invalid_argument(nullptr);
            }
        } catch (std::logic_error& ex) {
            std::cout << "incorrect option value: " << argv[2] << "\n";
            return -1;
        }

        in_filename = argv[3];
        out_filename = argv[4];
    }
    else {
        in_filename = argv[1];
        out_filename = argv[2];
    }

    std::regex url_re;
    try {
        url_re =
            "(https?://)([-a-zA-Z0-9\\.-]+)(\\s|$|/[-a-zA-Z0-9\\.,/\\+_]*)";
    } catch (std::regex_error& ex) {
        std::cout << "regex_error: " << ex.what() << '\n';
        return -1;
    }

    std::ifstream input(in_filename, std::ios::in);
    if (!input) {
        std::cout << "Error while opening file: " << in_filename << "\n";
        return -1;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();

    DomainPathCounter dpc(buffer.str(), url_re, static_cast<size_t>(n));
    auto stats = dpc.GetStats();

    std::ofstream output(out_filename, std::ios::out);
    if (!output) {
        std::cout << "Error while opening file: " << out_filename << "\n";
        return -1;
    }

    output << stats;
    if (!output) {
        std::cout << "Couldn't write to file: " << out_filename << "\n";
        return -1;
    }

    return 0;
}
