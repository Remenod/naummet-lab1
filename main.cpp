#include <iostream>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include <memory>
#include "tinyexpr.h"

constexpr double eps = 1e-12;

struct Config
{
    int derivative_subranges_count = 5;
    int bracketing_subranges_count = 10;
    double bracketing_precision = 0.01;
    double derivative_precision = 0.0005;
    double refining_precision = 0.01;
};

struct Range
{
    double begin;
    double end;

    friend std::ostream &operator<<(std::ostream &out, const Range &rng)
    {
        out << "(" << rng.begin << ", " << rng.end << ")";
        return out;
    }
};

double refine_roots(std::function<double(double)> func, Range range, Config &config)
{
    double fa = func(range.begin);

    while ((range.end - range.begin) > config.refining_precision)
    {
        double mid = (range.begin + range.end) / 2.0;
        double fm = func(mid);

        if (std::abs(fm) < config.refining_precision)
            return mid;

        if (fa * fm > 0)
        {
            range.begin = mid;
            fa = fm;
        }
        else
        {
            range.end = mid;
        }
    }

    return (range.begin + range.end) / 2.0;
}

std::vector<Range> bracket_roots(
    std::function<double(double)> func,
    Range range,
    Config &config,
    bool search_for_tangent_roots = false)
{
    auto func_derivative = [func, config](double x)
    {
        double dx = config.derivative_precision;
        return (func(x + dx) - func(x)) / dx;
    };

    auto is_derivative_change_sign = [func, func_derivative, config](Range range)
    {
        const double step = std::abs(range.end - range.begin) / config.derivative_subranges_count;
        const bool prev_sign = func_derivative(range.begin) > eps;

        for (int i = 0; i <= config.derivative_subranges_count; i++)
        {
            bool sign = func_derivative(range.begin + i * step) > eps;
            if (sign != prev_sign)
                return true;
        }
        return false;
    };

    std::vector<Range> result;
    const double step = std::abs(range.end - range.begin) / config.bracketing_subranges_count;

    for (int i = 0; i < config.bracketing_subranges_count; i++)
    {
        Range current_range =
            {
                .begin = range.begin + step * i,
                .end = range.begin + step * (i + 1),
            };

        if (is_derivative_change_sign(current_range) && step > config.bracketing_precision)
        {
            auto recursion_result = bracket_roots(func, current_range, config, true);
            result.insert(result.end(), recursion_result.begin(), recursion_result.end());
        }
        else
        {
            const double fa = func(current_range.begin);
            const double fb = func(current_range.end);

            const bool sign_change = fa * fb < 0;

            if (sign_change)
            {
                result.emplace_back(current_range);
            }
            else if (search_for_tangent_roots && !(step > config.bracketing_precision))
            {
                auto ranges_with_root = bracket_roots(func_derivative, current_range, config);

                for (auto range_with_root : ranges_with_root)
                {
                    auto derivative_root = refine_roots(func_derivative, range_with_root, config);

                    if (std::abs(func(derivative_root)) < config.bracketing_precision)
                        result.emplace_back(Range{derivative_root, derivative_root});
                }
            }
        }
    }

    return result;
}

int parse_config(Config *config)
{
    std::ifstream file("config.conf");
    std::string key, value;

    if (!file.is_open())
        return 1;

    while (file >> key)
    {
        auto pos = key.find('=');
        if (pos == std::string::npos)
            continue;

        value = key.substr(pos + 1);
        key = key.substr(0, pos);

        if (key == "derivative_check_subranges")
            config->derivative_subranges_count = std::stoi(value);
        else if (key == "derivative_precision")
            config->derivative_precision = std::stod(value);
        else if (key == "refining_precision")
            config->refining_precision = std::stod(value);
        else if (key == "bracketing_precision")
            config->bracketing_precision = std::stod(value);
        else if (key == "bracketing_check_subranges")
            config->bracketing_subranges_count = std::stoi(value);
    }
    return 0;
}

std::string get_expression(void)
{
    std::cout
        << "Enter an expression (e.g. "
        << "\033[4m"
        << "sin(x) - 0.5*cos(x^2)"
        << "\033[24m"
        << "):\n";

    std::string line;
    std::getline(std::cin, line);
    return line.size() < 1 ? "sin(x) - 0.5*cos(x^2)" : line;
}

std::function<double(double)> get_func(const std::string &expr)
{
    auto te_x = std::make_shared<double>(0);

    te_variable vars[] = {{"x", te_x.get(), TE_VARIABLE, nullptr}};
    int err;
    te_expr *e_raw = te_compile(expr.c_str(), vars, 1, &err);

    if (err)
    {
        std::string spaces(err - 1, ' ');
        std::cerr << "\033[31m"
                  << "\nAn error occured while parsing expression\n"
                  << expr << "\n"
                  << spaces << "^"
                  << "\033[0m"
                  << std::endl;
        return {};
    }

    auto e = std::shared_ptr<te_expr>(e_raw, [](te_expr *ptr)
                                      { te_free(ptr); });

    return [e, te_x](double x) mutable
    {
        *te_x = x;
        return te_eval(e.get());
    };
}

Range get_range(void)
{
    std::cout << "Enter operating range:" << std::endl;
    Range range;
    std::cin >> range.begin >> range.end;
    return range;
}

int main()
{
    Config config;
    if (parse_config(&config))
        std::cerr
            << "\033[33m"
            << "config.conf not fount. Using default values\n"
            << "\033[0m";

    auto func = get_func(get_expression());
    if (!func)
        return 1;

    auto range = get_range();

    std::vector<Range> bracketed_roots = bracket_roots(func, range, config, true);

    std::cout << "\033[32m"
              << "Found " << bracketed_roots.size()
              << ((bracketed_roots.size() == 1) ? " root" : " roots")
              << "\033[0m"
              << "\n";

    for (auto range_with_root : bracketed_roots)
    {
        auto root = refine_roots(func, range_with_root, config);
        std::cout << root
                  << "\033[90m"
                  << " on range " << range_with_root << "\n"
                  << "\033[0m";
    }

    return 0;
}
