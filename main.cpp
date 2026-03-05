#include <iostream>
#include <cmath>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include "tinyexpr.h"

#include <sstream>

constexpr double eps = 1e-12;

int _conf_derivative_check_subranges = 5;
int _conf_bracketing_check_subranges = 10;
double _conf_bracketing_precision = 0.01;
double _conf_derivative_precision = 0.0005;
double _conf_refining_precision = 0.01;

typedef struct
{
    double begin;
    double end;
} range_t;

double refine_roots(
    std::function<double(double)> func,
    range_t range,
    double precision)
{
    double fa = func(range.begin);
    double fb = func(range.end);

    if (fa * fb > 0)
        return (range.begin + range.end) / 2.0; // root existing is not guaranteed

    while ((range.end - range.begin) > precision)
    {
        double mid = (range.begin + range.end) / 2.0;
        double fm = func(mid);

        if (std::abs(fm) < eps)
            return mid;

        if (fa * fm > 0)
        {
            range.begin = mid;
            fa = fm;
        }
        else
        {
            range.end = mid;
            fb = fm;
        }
    }

    return (range.begin + range.end) / 2.0;
}

std::vector<range_t> bracket_roots(
    std::function<double(double)> func,
    range_t range,
    int bracketing_subranges_count = _conf_bracketing_check_subranges,
    double precision = _conf_bracketing_precision,
    bool search_for_tangent_roots = false)
{
    auto find_derivative = [func](double x, double dx = _conf_derivative_precision)
    {
        return (func(x + dx) - func(x)) / dx;
    };

    auto is_derivative_change_sign =
        [func, find_derivative](
            range_t range,
            int bracketing_subranges_count = _conf_derivative_check_subranges)
    {
        const double step = std::abs(range.end - range.begin) / bracketing_subranges_count;
        const bool prev_sign = find_derivative(range.begin) > 0;

        for (int i = 0; i <= bracketing_subranges_count; i++)
        {
            bool sign = find_derivative(range.begin + i * step) >= 0;
            if (sign != prev_sign)
                return true;
        }
        return false;
    };

    std::vector<range_t> result = {};
    const double step = std::abs(range.end - range.begin) / bracketing_subranges_count;

    for (int i = 0; i < bracketing_subranges_count; i++)
    {
        range_t current_range =
            {
                .begin = range.begin + step * i,
                .end = range.begin + step * (i + 1),
            };

        if (is_derivative_change_sign(current_range) && step > precision)
        {
            auto recursion_result = bracket_roots(func,
                                                  current_range,
                                                  bracketing_subranges_count,
                                                  precision,
                                                  true);

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
            else if (search_for_tangent_roots && !(step > precision))
            {
                auto ranges_with_root = bracket_roots(find_derivative,
                                                      current_range,
                                                      bracketing_subranges_count,
                                                      precision);

                for (auto range_with_root : ranges_with_root)
                {
                    auto derivative_root = refine_roots(func,
                                                        range_with_root,
                                                        precision);

                    if (std::abs(func(derivative_root)) < precision)
                        result.emplace_back(range_t{derivative_root, derivative_root});
                }
            }
        }
    }

    return result;
}

int parse_config(void)
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
            _conf_derivative_check_subranges = std::stoi(value);
        else if (key == "derivative_precision")
            _conf_derivative_precision = std::stod(value);
        else if (key == "refining_precision")
            _conf_refining_precision = std::stod(value);
        else if (key == "bracketing_precision")
            _conf_bracketing_precision = std::stod(value);
        else if (key == "bracketing_check_subranges")
            _conf_bracketing_check_subranges = std::stoi(value);
    }
    return 0;
}

int main()
{
    if (parse_config())
        std::cerr << "\033[33m"
                  << "config.conf not fount. Using default values\n"
                  << "\033[0m";

    std::cout
        << "Enter an expression (e.g. " << "\033[4m" << "sin(x) - 0.5*cos(x^2)" << "\033[24m" << "):" << std::endl;
    const char *expr;

    std::string line;
    std::getline(std::cin, line);
    expr = line.size() < 1 ? "sin(x) - 0.5*cos(x^2)" : line.c_str();

    double te_x = 0;
    te_variable vars[] = {{"x", &te_x, TE_VARIABLE, NULL}};
    int err;
    te_expr *e = te_compile(expr, vars, 1, &err);

    if (err)
    {
        std::string spaces(err - 1, ' ');
        std::cerr << "\033[31m"
                  << "\nAn error occured while parsing expression\n"
                  << line << "\n"
                  << spaces << "^"
                  << "\033[0m"
                  << std::endl;
        return 1;
    }

    auto func = [&e, &te_x](double x)
    {
        te_x = x;
        return te_eval(e);
    };

    std::cout << "Enter operating range:" << std::endl;
    range_t range = {};
    std::cin >> range.begin >> range.end;

    std::vector<range_t> bracketed_roots =
        bracket_roots(func,
                      range,
                      _conf_bracketing_check_subranges,
                      _conf_bracketing_precision, true);

    std::cout << "\033[32m"
              << "Found " << bracketed_roots.size()
              << ((bracketed_roots.size() == 1) ? " root" : " roots")
              << "\033[0m"
              << "\n";

    for (auto range_with_root : bracketed_roots)
    {
        auto root = refine_roots(func, range_with_root, _conf_refining_precision);
        std::cout << root
                  << "\033[90m"
                  << " on range (" << range_with_root.begin << ", " << range_with_root.end << ")\n"
                  << "\033[0m";
    }

    te_free(e);
    return 0;
}
