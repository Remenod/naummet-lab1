#include <iostream>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include <memory>
#include "lib/tinyexpr.h"

typedef std::function<double(double)> mathfunc;

constexpr double eps = 1e-12;

struct Config
{
    int derivative_subranges_count = 5;
    int bracketing_subranges_count = 10;
    double derivative_precision = 1e-3;
    double refining_precision = 1e-2;
    double bracketing_precision = 1e-2;
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

double refine_root(mathfunc func, Range range, Config &config)
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

std::vector<Range> bracket_roots(mathfunc func, Range range, Config &config, bool search_for_tangent = true)
{
    auto find_derivative_value = [](mathfunc func, double x, double dx)
    {
        return (func(x + dx) - func(x)) / dx;
    };

    auto func_derivative = [func, config, find_derivative_value](double x)
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

        const bool derivative_change_sign = is_derivative_change_sign(current_range);

        if (derivative_change_sign && (step > config.bracketing_precision))
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
            else if (derivative_change_sign && search_for_tangent)
            {
                auto ranges_with_extremum = bracket_roots(func_derivative, current_range, config, false);

                for (auto range_with_extremum : ranges_with_extremum)
                {
                    auto derivative_root = refine_root(func_derivative, range_with_extremum, config);

                    auto is_root = [func, func_derivative, find_derivative_value, config](double x)
                    {
                        double second =
                            find_derivative_value(func_derivative, x, config.derivative_precision);

                        double C = std::abs(second) * 0.5 + eps;

                        return std::abs(func(x)) <
                               C * (config.refining_precision / 2) * (config.refining_precision / 2);
                    };

                    if (is_root(derivative_root))
                        result.emplace_back(Range{derivative_root, derivative_root});
                }
            }
        }
    }

    return result;
}

static int parse_config(Config *config)
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

static std::string get_expression(void)
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

static mathfunc get_func(const std::string &expr, int &err)
{
    auto te_x = std::make_shared<double>(0);

    te_variable vars[] = {{"x", te_x.get(), TE_VARIABLE, nullptr}};
    te_expr *e_raw = te_compile(expr.c_str(), vars, 1, &err);

    if (err)
        return {};

    auto e = std::shared_ptr<te_expr>(e_raw, [](te_expr *ptr)
                                      { te_free(ptr); });

    return [e, te_x](double x)
    {
        *te_x = x;
        return te_eval(e.get());
    };
}

static Range get_range(void)
{
    std::cout << "Enter operating range:" << "\n";
    Range range;
    std::cin >> range.begin >> range.end;
    return range;
}

static void print_config_status(int code)
{
    switch (code)
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
}

static void print_expression_error(const std::string &expr, int err)
{
    std::string spaces(err - 1, ' ');
    std::cerr << "\033[31m"
              << "\nAn error occured while parsing expression\n"
              << expr << "\n"
              << spaces << "^"
              << "\033[0m\n";
}

static void print_roots_found(int root_count)
{
    std::cout << "\033[32m"
              << "Found " << root_count
              << ((root_count == 1) ? " root" : " roots")
              << "\033[0m"
              << "\n";
}

int main()
{
    Config config;
    print_config_status(parse_config(&config));

    auto expr = get_expression();

    int func_err;
    auto func = get_func(expr, func_err);

    if (func_err)
    {
        print_expression_error(expr, func_err);
        return 1;
    }

    auto range = get_range();

    auto bracketed_roots = bracket_roots(func, range, config, false);

    print_roots_found(bracketed_roots.size());

    for (const auto &range_with_root : bracketed_roots)
    {
        auto root = refine_root(func, range_with_root, config);
        std::cout << root
                  << "\033[90m"
                  << " on range " << range_with_root << "\n"
                  << "\033[0m";
    }

    return 0;
}
