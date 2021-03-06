﻿// 
// Copyright (c) Microsoft Corporation. All rights reserved.
// 
// Microsoft Public License (MS-PL)
// 
// This license governs use of the accompanying software. If you use the
// software, you accept this license. If you do not accept the license, do not
// use the software.
// 
// 1. Definitions
// 
//   The terms "reproduce," "reproduction," "derivative works," and
//   "distribution" have the same meaning here as under U.S. copyright law. A
//   "contribution" is the original software, or any additions or changes to
//   the software. A "contributor" is any person that distributes its
//   contribution under this license. "Licensed patents" are a contributor's
//   patent claims that read directly on its contribution.
// 
// 2. Grant of Rights
// 
//   (A) Copyright Grant- Subject to the terms of this license, including the
//       license conditions and limitations in section 3, each contributor
//       grants you a non-exclusive, worldwide, royalty-free copyright license
//       to reproduce its contribution, prepare derivative works of its
//       contribution, and distribute its contribution or any derivative works
//       that you create.
// 
//   (B) Patent Grant- Subject to the terms of this license, including the
//       license conditions and limitations in section 3, each contributor
//       grants you a non-exclusive, worldwide, royalty-free license under its
//       licensed patents to make, have made, use, sell, offer for sale,
//       import, and/or otherwise dispose of its contribution in the software
//       or derivative works of the contribution in the software.
// 
// 3. Conditions and Limitations
// 
//   (A) No Trademark License- This license does not grant you rights to use
//       any contributors' name, logo, or trademarks.
// 
//   (B) If you bring a patent claim against any contributor over patents that
//       you claim are infringed by the software, your patent license from such
//       contributor to the software ends automatically.
// 
//   (C) If you distribute any portion of the software, you must retain all
//       copyright, patent, trademark, and attribution notices that are present
//       in the software.
// 
//   (D) If you distribute any portion of the software in source code form, you
//       may do so only under this license by including a complete copy of this
//       license with your distribution. If you distribute any portion of the
//       software in compiled or object code form, you may only do so under a
//       license that complies with this license.
// 
//   (E) The software is licensed "as-is." You bear the risk of using it. The
//       contributors give no express warranties, guarantees or conditions. You
//       may have additional consumer rights under your local laws which this
//       license cannot change. To the extent permitted under your local laws,
//       the contributors exclude the implied warranties of merchantability,
//       fitness for a particular purpose and non-infringement.
//       

#pragma once

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------

template <typename T>
class WeakRefTarget;

template <typename T>
class WeakRefImpl;

//-----------------------------------------------------------------------------
// WeakRef
//-----------------------------------------------------------------------------

template <typename T>
class WeakRef
{
    friend class WeakRefTarget<T>;

public:

    SharedPtr<T> GetTarget() const
    {
        return m_spImpl->GetTarget();
    }

private:

    explicit WeakRef(WeakRefImpl<T>* pImpl):
        m_spImpl(pImpl)
    {
    }

    SharedPtr<WeakRefImpl<T>> m_spImpl;
};

//-----------------------------------------------------------------------------
// WeakRefUtil
//-----------------------------------------------------------------------------

template <typename T>
class WeakRefUtil
{
    PROHIBIT_CONSTRUCT(WeakRefUtil)

    friend class WeakRefTarget<T>;
    friend class WeakRefImpl<T>;

private:

    template <typename TResult>
    static TResult CallWithLock(const function<TResult()>& callback)
    {
        BEGIN_MUTEX_SCOPE(*ms_pMutex)

            return callback();

        END_MUTEX_SCOPE
    }

    // Put the mutex on the heap. At process shutdown, static cleanup races against GC,
    // so using non-POD static data in conjunction with managed objects is problematic.

    static SimpleMutex* ms_pMutex;
};

template <typename T>
SimpleMutex* WeakRefUtil<T>::ms_pMutex = new SimpleMutex;

//-----------------------------------------------------------------------------
// WeakRefTarget
//-----------------------------------------------------------------------------

template <typename T>
class WeakRefTarget: public SharedPtrTarget
{
public:

    WeakRef<T> CreateWeakRef()
    {
        return WeakRefUtil<T>::CallWithLock<WeakRef<T>>([this]
        {
            if (m_spWeakRefImpl.IsEmpty())
            {
                m_spWeakRefImpl = new WeakRefImpl<T>(static_cast<T*>(this));
            }

            return WeakRef<T>(m_spWeakRefImpl);
        });
    }

protected:

    ~WeakRefTarget()
    {
        if (!m_spWeakRefImpl.IsEmpty())
        {
            m_spWeakRefImpl->OnTargetDeleted();
        }
    }

private:

    SharedPtr<WeakRefImpl<T>> m_spWeakRefImpl;
};

//-----------------------------------------------------------------------------
// WeakRefImpl
//-----------------------------------------------------------------------------

template <typename T>
class WeakRefImpl: public SharedPtrTarget
{
    friend class WeakRef<T>;
    friend class WeakRefTarget<T>;

private:

    explicit WeakRefImpl(T* pTarget):
        m_pTarget(pTarget)
    {
    }

    SharedPtr<T> GetTarget() const
    {
        return WeakRefUtil<T>::CallWithLock<SharedPtr<T>>([this]
        {
            SharedPtr<T> spTarget;

            if (m_pTarget != nullptr)
            {
                AddRefScope addRefScope(m_pTarget->GetRefCount());
                if (addRefScope.GetRefCountValue() > 1)
                {
                    spTarget = m_pTarget;
                }
            }

            return spTarget;
        });
    }

    void OnTargetDeleted()
    {
        WeakRefUtil<T>::CallWithLock<void>([this]
        {
            m_pTarget = nullptr;
        });
    }

    T* m_pTarget;
};
