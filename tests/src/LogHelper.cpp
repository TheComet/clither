#include "clither/tests/LogHelper.hpp"

extern "C" {
#include "clither/util/cli_colors.h"
}

/* ------------------------------------------------------------------------- */
int diffPos(const std::string& a, const std::string& b)
{
    size_t pos = 0;
    for (; pos < a.size() && pos < b.size(); ++pos)
        if (a[pos] != b[pos])
            break;
    return (int)pos;
}

/* ------------------------------------------------------------------------- */
bool LogEqMatcher::MatchAndExplain(
    const LogOutput& logOutput, testing::MatchResultListener* listener) const
{
    int         pos = diffPos(logOutput.text, expected);
    std::string highlighted = logOutput.text;
    highlighted.insert(pos, COL_B_RED);
    highlighted.append(COL_RESET);
    std::replace(highlighted.begin(), highlighted.end(), ' ', '.');
    *listener << "Log:\n" << highlighted;
    return logOutput.text == expected;
}
void LogEqMatcher::DescribeTo(::std::ostream* os) const
{
    std::string highlighted = expected;
    std::replace(highlighted.begin(), highlighted.end(), ' ', '.');
    *os << "Log output equals:\n" << highlighted;
}
void LogEqMatcher::DescribeNegationTo(::std::ostream* os) const
{
    std::string highlighted = expected;
    std::replace(highlighted.begin(), highlighted.end(), ' ', '.');
    *os << "Log output does not equal:\n" << highlighted;
}

/* ------------------------------------------------------------------------- */
bool LogStartsWithMatcher::MatchAndExplain(
    const LogOutput& logOutput, testing::MatchResultListener* listener) const
{
    *listener << "Log:\n" << logOutput.text;
    return logOutput.text.find(expected, 0) == 0;
}
void LogStartsWithMatcher::DescribeTo(::std::ostream* os) const
{
    std::string highlighted = expected;
    std::replace(highlighted.begin(), highlighted.end(), ' ', '.');
    *os << "Log output starts with:\n" << highlighted;
}
void LogStartsWithMatcher::DescribeNegationTo(::std::ostream* os) const
{
    std::string highlighted = expected;
    std::replace(highlighted.begin(), highlighted.end(), ' ', '.');
    *os << "Log output does not start with:\n" << highlighted;
}

/* ------------------------------------------------------------------------- */
static LogOutput log_output;
static void      write_str(const char* fmt, va_list ap)
{
    va_list ap2;
    va_copy(ap2, ap);
    int len = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);

    size_t off = log_output.text.size();
    log_output.text.resize(log_output.text.size() + len + 1);
    vsprintf(log_output.text.data() + off, fmt, ap);
    log_output.text.resize(log_output.text.size() - 1);
}
static void flush_str(void)
{
}
LogHelper::LogHelper()
{
    struct log_interface i = {
        /*write=*/write_str,
        /*flush=*/flush_str,
        /*prefix=*/"",
        /*set_color=*/"",
        /*clear_color=*/"",
        /*use_color=*/0,
    };
    old_log_interface = log_configure(i);
}
LogHelper::~LogHelper()
{
    log_configure(old_log_interface);
    log_output.text.clear();
}
const LogOutput& LogHelper::log() const
{
    return log_output;
}
