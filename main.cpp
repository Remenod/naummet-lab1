#include <iostream>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include <memory>
#include <cmath>
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

static double find_derivative_value(std::function<double(double)> func, double x, double dx)
{
    return (func(x + dx) - func(x)) / dx;
}

double refine_root(std::function<double(double)> func, Range range, Config &config)
{
    double fa = func(range.begin);

    while ((range.end - range.begin) > config.refining_precision)
    {
        double mid = (range.begin + range.end) / 2.0;
        double fm = func(mid);

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
        return find_derivative_value(func, x, config.derivative_precision);
    };

    auto is_derivative_change_sign = [func, func_derivative, config](Range range)
    {
        const double step = std::abs(range.end - range.begin) / config.derivative_subranges_count;
        const bool prev_sign = func_derivative(range.begin) > std::min(eps, config.derivative_precision);

        for (int i = 0; i <= config.derivative_subranges_count; i++)
        {
            bool sign = func_derivative(range.begin + i * step) > std::min(eps, config.derivative_precision);
            if (sign != prev_sign)
                return true;
        }
        return false;
    };

    std::vector<Range> result;
    const double step = std::abs(range.end - range.begin) / config.bracketing_subranges_count;

    for (int i = 0; i < config.bracketing_subranges_count; i++)
    {
        Range current_range = {
            .begin = range.begin + step * i,
            .end = range.begin + step * (i + 1),
        };

        if (is_derivative_change_sign(current_range) && (step > config.bracketing_precision))
        {
            auto recursion_result = bracket_roots(func, current_range, config, true);
            result.insert(result.end(), recursion_result.begin(), recursion_result.end());
        }
        else
        {
            const bool sign_change = func(current_range.begin) * func(current_range.end) < 0;

            if (sign_change)
            {
                result.emplace_back(current_range);
            }
            else if (search_for_tangent_roots && is_derivative_change_sign(current_range))
            {
                auto ranges_with_root = bracket_roots(func_derivative, current_range, config);

                for (auto range_with_root : ranges_with_root)
                {
                    auto derivative_root = refine_root(func_derivative, range_with_root, config);

                    auto is_root = [func, func_derivative, config](double x)
                    {
                        double second =
                            find_derivative_value(func_derivative, x, config.derivative_precision);

                        double C = std::abs(second) * 0.5 + eps;

                        return std::abs(func(x)) < C * std::pow((config.refining_precision), 2);
                    };

                    if (is_root(derivative_root))
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

        if (key == "derivative_subranges_count")
            config->derivative_subranges_count = std::stoi(value);
        else if (key == "derivative_precision")
            config->derivative_precision = std::stod(value);
        else if (key == "refining_precision")
            config->refining_precision = std::stod(value);
        else if (key == "bracketing_precision")
            config->bracketing_precision = std::stod(value);
        else if (key == "bracketing_subranges_count")
            config->bracketing_subranges_count = std::stoi(value);
    }

    if (config->derivative_precision < 1e-8 - eps)
        return 2; // derivative_precision NOT >= 1e-8

    if (config->refining_precision <= config->derivative_precision + eps)
        return 3; // refining_precision NOT > derivative_precision

    if (config->bracketing_precision < config->refining_precision - eps)
        return 4; // bracketing_precision NOT >= refining_precision
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

    return [e, te_x](double x)
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
    switch (parse_config(&config))
    {
    case 1:
        std::cerr << "\033[33m"
                  << "config.conf not found.\n"
                     "Using default values."
                  << "\033[0m\n";
        break;
    case 2:
        std::cerr << "\033[31m"
                  << "Derivative precision is smaller than the recommended minimum (1e-8).\n"
                     "Floating-point calculation noise may corrupt the output."
                  << "\033[0m\n";
        break;
    case 3:
        std::cerr << "\033[33m"
                  << "Refining precision must be larger (less accurate) than derivative precision."
                  << "\033[0m\n";
        break;
    case 4:
        std::cerr << "\033[33m"
                  << "Bracketing precision must be larger (less accurate) than or equal to refining precision."
                  << "\033[0m\n";
        break;
    }

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
        auto root = refine_root(func, range_with_root, config);
        std::cout << root
                  << "\033[90m"
                  << " on range " << range_with_root << "\n"
                  << "\033[0m";
    }

    return 0;
}
