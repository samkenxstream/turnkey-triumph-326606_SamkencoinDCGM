/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <functional>
#include <utility>


namespace DcgmNs
{
template <typename Func>
struct [[nodiscard]] Defer final
{
    explicit Defer(Func &&f)
        : m_func(std::forward<Func>(f))
    {}

    ~Defer()
    {
        if (!m_disarmed)
        {
            try
            {
                std::invoke(m_func);
            }
            catch (...)
            {}
        }
    }

    void Disarm()
    {
        m_disarmed = true;
    }

private:
    Func m_func;
    bool m_disarmed = false;
};
} // namespace DcgmNs
