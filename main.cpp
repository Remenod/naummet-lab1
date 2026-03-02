#include <iostream>
#include <cmath>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include "tinyexpr.h"

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

std::vector<range_t> bracket_roots(
    std::function<double(double)> func,
    range_t range,
    int separation_parts,
    double precision)
{
    std::vector<range_t> result = {};
    const double step = std::abs(range.end - range.begin) / separation_parts;

    auto is_derivative_change_sign = [func](range_t range, int separation_parts)
    {
        auto find_derivative = [func](double x, double dx)
        {
            return (func(x + dx) - func(x)) / dx;
        };

        const double step = std::abs(range.end - range.begin) / separation_parts;

        const bool prev_sign = find_derivative(range.begin, _conf_derivative_precision) >= 0;
        for (int i = 0; i <= separation_parts; i++)
        {
            double d = find_derivative(range.begin + i * step, _conf_derivative_precision);

            if (std::abs(d) < eps)
                return true;

            bool sign = d >= 0;
            if (sign != prev_sign)
                return true;

            if (std::abs(d) < eps ||
                std::abs(find_derivative(range.begin + (i + 1) * step, _conf_derivative_precision)) < eps)
                return true;
        }
        return false;
    };

    for (int i = 0; i < separation_parts; i++)
    {
        range_t current_range =
            {
                .begin = range.begin + step * i,
                .end = range.begin + step * (i + 1),
            };

        if (is_derivative_change_sign(current_range, _conf_derivative_check_subranges) && step > precision)
        {
            auto recursion_result = bracket_roots(func, current_range, separation_parts, precision);
            result.insert(result.end(), recursion_result.begin(), recursion_result.end());
        }
        else
        {
            const double fa = func(current_range.begin);
            const double fb = func(current_range.end);

            const bool b_is_zero = std::abs(fb) < eps;

            const bool sign_change = fa * fb < 0;

            auto is_duplicate_zero = [&](double x)
            {
                if (result.empty())
                    return false;

                return std::abs(result.back().end - x) < eps;
            };

            if (sign_change || (b_is_zero && !is_duplicate_zero(current_range.end)))
                result.emplace_back(current_range);
        }
    }

    return result;
}

double refine_roots(
    std::function<double(double)> func,
    range_t range,
    double precision)
{
    double fa = func(range.begin);
    double fb = func(range.end);

    if (fa * fb > 0)
        return (range.begin + range.end) / 2.0; // root exesting is not garateed

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
        std::cerr << "config.conf not fount. Using default values\n";

    std::cout << "Enter an expression (e.g., sin(x) - 0.5*cos(x^2)):" << std::endl;
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

    std::cout << "Enter operating range" << std::endl;
    range_t range = {};
    std::cin >> range.begin >> range.end;

    std::vector<range_t> bracketed_roots =
        bracket_roots(func, range, _conf_bracketing_check_subranges, _conf_bracketing_precision);

    auto bracketed_roots_size = bracketed_roots.size();
    std::cout << "Found " << bracketed_roots_size
              << ((bracketed_roots_size == 1) ? " root" : " roots")
              << "\n";
    for (auto el : bracketed_roots)
    {
        auto root = refine_roots(func, el, _conf_refining_precision);
        std::cout << root << " on range (" << el.begin << ", " << el.end << ")\n";
    }
    te_free(e);
    return 0;
}
