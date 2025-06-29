// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

struct ResultBase
{
    friend ResultBase Err(long errorCode);

  protected:
    const long errorCode;

    ResultBase() : errorCode(0) {};
    ResultBase(long errorCode) : errorCode(errorCode) {};

  public:
    virtual bool isErr() const final
    {
        return errorCode != 0;
    }

    virtual long getErr() const final
    {
        return errorCode;
    }

    explicit operator bool() const
    {
        return !isErr();
    }

    auto match(auto &&onOk, auto &&onErr) const
    {
        if (isErr())
            return onErr(errorCode);
        else
            return onOk();
    }
};

inline ResultBase Err(long errorCode)
{
    return ResultBase(errorCode);
}
