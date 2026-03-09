#include <iostream>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include <memory>
#include "lib/tinyexpr.h"

typedef std::function<double(double)> mathfunc;

constexpr double eps = 1e-12;
constexpr char default_expr[] = "sin(x) - 0.5*cos(x^2)";

struct config
{
    int derivative_subranges_count = 5;
    int bracketing_subranges_count = 10;
    double derivative_precision = 1e-3;
    double refining_precision = 1e-2;
    double bracketing_precision = 1e-2;
};

enum class config_status
{
    OK,
    FILE_NOT_FOUND,
    NOT_ENOUGH_BRACKETING_SUBRANGES,
    DERIVATIVE_PRECISION_TOO_SMALL,
    REFINING_PRECISION_TOO_SMALL,
    BRACKETING_PRECISION_TOO_SMALL,
};

struct range
{
    double begin;
    double end;

    friend std::ostream &operator<<(std::ostream &out, const range &rng)
    {
        out << "(" << rng.begin << ", " << rng.end << ")";
        return out;
    }
};

double refine_root(mathfunc func, range rng, config &cfg)
{
    double fa = func(rng.begin);

    while ((rng.end - rng.begin) > cfg.refining_precision)
    {
        double mid = (rng.begin + rng.end) / 2.0;
        double fm = func(mid);

        if (fa * fm > 0)
        {
            rng.begin = mid;
            fa = fm;
        }
        else
        {
            rng.end = mid;
        }
    }

    return (rng.begin + rng.end) / 2.0;
}

std::vector<range> bracket_roots(mathfunc func, range rng, config &cfg, bool search_for_tangent = true)
{
    auto find_derivative_value = [](mathfunc func, double x, double dx)
    {
        return (func(x + dx) - func(x)) / dx;
    };

    auto func_derivative = [func, cfg, find_derivative_value](double x)
    {
        return find_derivative_value(func, x, cfg.derivative_precision);
    };

    auto is_derivative_change_sign = [func, func_derivative, cfg](range rng)
    {
        const double step = std::abs(rng.end - rng.begin) / cfg.derivative_subranges_count;
        const bool prev_sign = func_derivative(rng.begin) >= eps;

        for (int i = 0; i <= cfg.derivative_subranges_count; i++)
        {
            bool sign = func_derivative(rng.begin + i * step) >= 0;
            if (sign != prev_sign)
                return true;
        }
        return false;
    };

    std::vector<range> result;
    const double step = std::abs(rng.end - rng.begin) / cfg.bracketing_subranges_count;

    for (int i = 0; i < cfg.bracketing_subranges_count; i++)
    {
        range current_range = {
            .begin = rng.begin + step * i,
            .end = rng.begin + step * (i + 1),
        };

        const bool derivative_change_sign = is_derivative_change_sign(current_range);
        const bool func_change_sign = func(current_range.begin + eps) * func(current_range.end) <= 0;

        if (derivative_change_sign)
        {
            if (step > cfg.bracketing_precision)
            {
                auto recursion_result = bracket_roots(func, current_range, cfg, search_for_tangent);
                result.insert(result.end(), recursion_result.begin(), recursion_result.end());
            }
            else if (search_for_tangent)
            {
                auto ranges_with_extremum = bracket_roots(func_derivative, current_range, cfg, false);

                for (auto range_with_extremum : ranges_with_extremum)
                {
                    auto derivative_root = refine_root(func_derivative, range_with_extremum, cfg);

                    auto is_root = [func, func_derivative, find_derivative_value, cfg](double x)
                    {
                        double second = find_derivative_value(func_derivative, x, cfg.derivative_precision);
                        double C = std::abs(second) * 0.5 + eps;
                        double delta = cfg.refining_precision / 2;

                        return std::abs(func(x)) < C * delta * delta;
                    };

                    if (is_root(derivative_root))
                        result.emplace_back(range{derivative_root, derivative_root});
                }
            }
        }
        else if (func_change_sign)
        {
            result.emplace_back(current_range);
        }
    }

    return result;
}

static config_status parse_config(config *cfg)
{
    std::ifstream file("config.conf");
    std::string key, value;

    if (!file.is_open())
        return config_status::FILE_NOT_FOUND;

    while (file >> key)
    {
        auto pos = key.find('=');
        if (pos == std::string::npos)
            continue;

        value = key.substr(pos + 1);
        key = key.substr(0, pos);

        if (key == "derivative_subranges_count")
            cfg->derivative_subranges_count = std::stoi(value);
        else if (key == "derivative_precision")
            cfg->derivative_precision = std::stod(value);
        else if (key == "refining_precision")
            cfg->refining_precision = std::stod(value);
        else if (key == "bracketing_precision")
            cfg->bracketing_precision = std::stod(value);
        else if (key == "bracketing_subranges_count")
            cfg->bracketing_subranges_count = std::stoi(value);
    }

    if (cfg->derivative_subranges_count < 2)
        return config_status::NOT_ENOUGH_BRACKETING_SUBRANGES;

    if (cfg->derivative_precision < 1e-8 - eps)
        return config_status::DERIVATIVE_PRECISION_TOO_SMALL; // derivative_precision NOT >= 1e-8

    if (cfg->refining_precision <= cfg->derivative_precision + eps)
        return config_status::REFINING_PRECISION_TOO_SMALL; // refining_precision NOT > derivative_precision

    if (cfg->bracketing_precision < cfg->refining_precision - eps)
        return config_status::REFINING_PRECISION_TOO_SMALL; // bracketing_precision NOT >= refining_precision
    return config_status::OK;
}

static std::string get_expression(void)
{
    std::cout
        << "Enter an expression (e.g. "
        << "\033[4m"
        << default_expr
        << "\033[24m):\n";

    std::string line;
    std::getline(std::cin, line);
    return line.size() < 1 ? default_expr : line;
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

static range get_range(void)
{
    std::cout << "Enter operating range:" << "\n";
    range rng;
    std::cin >> rng.begin >> rng.end;
    return rng;
}

static void print_config_status(config_status code)
{
    switch (code)
    {
    case config_status::OK:
        break;
    case config_status::FILE_NOT_FOUND:
        std::cerr << "\033[33m"
                  << "config.conf not found.\n"
                     "Using default values."
                  << "\033[0m\n";
        break;
    case config_status::NOT_ENOUGH_BRACKETING_SUBRANGES:
        std::cerr << "\033[31m"
                     "Bracketing subranges count is lower than 2.\n"
                     "At least two subranges are required to perform root bracketing."
                  << "\033[0m\n";
        break;
    case config_status::DERIVATIVE_PRECISION_TOO_SMALL:
        std::cerr << "\033[31m"
                  << "Derivative precision is smaller than the recommended minimum (1e-8).\n"
                     "Floating-point calculation noise may corrupt the output."
                  << "\033[0m\n";
        break;
    case config_status::REFINING_PRECISION_TOO_SMALL:
        std::cerr << "\033[33m"
                  << "Refining precision must be larger (less accurate) than derivative precision."
                  << "\033[0m\n";
        break;
    case config_status::BRACKETING_PRECISION_TOO_SMALL:
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
              << "\nAn error occurred while parsing expression\n"
              << expr << "\n"
              << spaces << "^"
              << "\033[0m\n";
}

static void print_roots_found(int root_count)
{
    std::cout << "\033[32m"
              << "Found " << root_count << ((root_count == 1) ? " root" : " roots")
              << "\033[0m\n";
}

int main()
{
    config cfg;
    auto cfg_status = parse_config(&cfg);
    print_config_status(cfg_status);
    if (cfg_status == config_status::NOT_ENOUGH_BRACKETING_SUBRANGES)
        return 2;

    auto expr = get_expression();

    int func_err;
    auto func = get_func(expr, func_err);

    if (func_err)
    {
        print_expression_error(expr, func_err);
        return 1;
    }

    auto rng = get_range();

    auto bracketed_roots = bracket_roots(func, rng, cfg);

    print_roots_found(bracketed_roots.size());

    for (const auto &range_with_root : bracketed_roots)
    {
        auto root = refine_root(func, range_with_root, cfg);
        std::cout << root
                  << "\033[90m"
                  << " on range " << range_with_root << "\n"
                  << "\033[0m";
    }

    return 0;
}
