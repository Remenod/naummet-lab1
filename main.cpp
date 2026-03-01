#include <iostream>
#include <cmath>
#include <vector>
#include <functional>
#include <tuple>
#include "tinyexpr.h"

constexpr double eps = 1e-12;

typedef struct
{
    double begin;
    double end;
} range_t;

std::vector<range_t> bracket_roots(
    std::function<double(double)> func,
    range_t range,
    int separation_parts = 10)
{
    std::vector<range_t> result = {};
    double step = std::abs(range.end - range.begin) / separation_parts;

    std::vector<range_t> ranges_with_root = {};

    bool prev_sign = func(range.begin) > eps;

    for (double i = range.begin; i < range.end; i += step)
    {
        int sign = func(i) > eps;

        if (sign != prev_sign)
        {
            prev_sign = sign;
            ranges_with_root.push_back(
                {
                    .begin = i - step,
                    .end = i,
                });
        }
    }

    auto is_have_multiple_roots = [func](range_t range, int separation_parts = 5)
    {
        auto find_derivative = [func](double x, double dx = 0.005)
        {
            return (func(x + dx) - func(x)) / dx;
        };

        double step = std::abs(range.end - range.begin) / separation_parts;

        bool prev_sign = find_derivative(range.begin) > 0;
        for (double i = range.begin; i < range.end; i += step)
        {
            int sign = find_derivative(i) > eps;

            if (sign != prev_sign)
                return true;
        }
        return false;
    };

    for (auto el : ranges_with_root)
    {
        if (!is_have_multiple_roots(el))
        {
            result.push_back(el);
        }
        else
        {
            auto recursion_result = bracket_roots(func, el);
            result.insert(result.end(), recursion_result.begin(), recursion_result.end());
        }
    }

    return result;
}

double refine_roots(
    std::function<double(double)> func,
    range_t range,
    double precision = 0.01)
{
    bool func_sign_on_range_begin = func(range.begin) > eps;
    bool func_sign_on_range_end = func(range.end) > eps;

    if (func_sign_on_range_begin == func_sign_on_range_end)
        return NAN;

    while ((range.end - range.begin) > precision)
    {
        double middle_point = range.begin + (range.end - range.begin) / 2.0;
        double func_on_middle_point = func(middle_point);

        if (std::abs(func_on_middle_point) < eps)
            return middle_point;

        bool func_sign_on_middle = func_on_middle_point > eps;

        if (func_sign_on_middle == func_sign_on_range_begin)
            range.begin = middle_point;
        else
            range.end = middle_point;
    }

    return (range.begin + range.end) / 2.0;
}

int main()
{
    std::cout << "Enter an expression (e.g., sin(x) - 0.5*cos(x^2)):" << std::endl;
    const char *expr;
    std::string line;
    std::getline(std::cin, line);
    expr = line.c_str();

    double te_x = 0.5;
    te_variable vars[] = {{"x", &te_x, TE_VARIABLE, NULL}};
    te_expr *e = te_compile(expr, vars, 1, NULL);

    if (e == nullptr)
    {
        std::cout << "An error occured while parsing expression" << std::endl;
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

    std::vector<range_t> separated_roots = bracket_roots(func, range, 10);

    for (auto el : separated_roots)
        std::cout << "(" << el.begin << ", " << el.end << ")  ->   " << refine_roots(func, el) << "\n";

    te_free(e);
    return 0;
}