#pragma once

#include "LM.h"
#include "ReferenceCounter.h"
#include <memory>

namespace Lumos
{
    class LUMOS_EXPORT ReferenceBase
    {
    public:
        ReferenceBase();
        ~ReferenceBase();
        
        inline bool IsReferenced() const { return m_RefcountInit.get() < 1; }
        bool InitRef();
        bool reference(); // returns false if refcount is at zero and didn't get increased
        bool unreference();
        int GetReferenceCount() const;
    protected:
        static void _bind_methods();
    private:
        ReferenceCounter m_Refcount;
        ReferenceCounter m_RefcountInit;
    };
    
    template<class T>
    class LUMOS_EXPORT Reference
    {
    public:
        Reference(std::nullptr_t)
        {
            m_Ptr = nullptr;
            m_Counter = new ReferenceBase();
            m_Counter->InitRef();
        }
        
        Reference(T* ptr = nullptr)
        {
            m_Ptr = ptr;
            m_Counter = new ReferenceBase();
            m_Counter->InitRef();
        }
        
        Reference(const Reference& other)
        {
            m_Ptr = other.m_Ptr;
            m_Counter = other.m_Counter;
            m_Counter->reference();
        }
        
        ~Reference()
        {
            if(m_Counter)
            {
                if(m_Counter->unreference())
                {
                    delete m_Counter;
                    if(m_Ptr)
                        delete m_Ptr;
                }
            }
        }
        
        void operator=(Reference const& rhs)
        {
            // Keep a copy of the old data
            T*   oldData  = m_Ptr;
            ReferenceBase* oldCount = m_Counter;
            
            // now we do an exception safe transfer;
            m_Ptr  = rhs.m_Ptr;
            m_Counter = rhs.m_Counter;
            
            // Update the counters
            m_Counter->reference();
            
            // Finally delete the old pointer if required.
            if(oldData)
            {
                if (oldCount->unreference())
                {
                    delete oldData;
                }
            }
        }
        
        inline T* release() noexcept
        {
            T* tmp = nullptr;
            m_Counter->unreference();
            std::swap(tmp, m_Ptr);
            m_Ptr = nullptr;
            return tmp;
        }
        
        inline void reset(T *p_ptr = nullptr)
        {
            if(m_Counter->unreference())
            {
                delete m_Ptr;
            }
            
            delete m_Counter;
            
            m_Ptr = p_ptr;
            m_Counter = new ReferenceBase();
            m_Counter->InitRef();
        }
        
        template<typename U>
        Reference(Reference<U>&& moving)
        {
            Reference<T> tmp(moving.release());
            tmp.swap(*this);
        }
        
        template<typename U>
        Reference& operator=(Reference<U>&& moving)
        {
            Reference<T> tmp(moving.release());
            tmp.swap(*this);
            return *this;
        }
        
        inline bool operator==(const T *p_ptr) const
        {
            return m_Ptr == p_ptr;
        }
        
        inline bool operator!=(const T *p_ptr) const
        {
            return m_Ptr != p_ptr;
        }
        
        inline bool operator<(const Reference<T> &p_r) const
        {
            return m_Ptr < p_r.m_Ptr;
        }
            
        inline bool operator==(const Reference<T> &p_r) const
        {
            return m_Ptr == p_r.m_Ptr;
        }
            
        inline bool operator!=(const Reference<T> &p_r) const
        {
            return m_Ptr != p_r.m_Ptr;
        }
            
        void swap(Reference& other) noexcept
        {
            std::swap(m_Ptr,  other.m_Ptr);
            std::swap(m_Counter, other.m_Counter);
        }
        // Const correct access owned object
        T* operator->() const {return m_Ptr;}
        T& operator*()  const {return *m_Ptr;}
        
        // Access to smart pointer state
        T* get()                 const {return m_Ptr;}
        explicit operator bool() const {return m_Ptr;}
        
    private:
        ReferenceBase* m_Counter = nullptr;
        T* m_Ptr;
    };
    
    template<class T>
    class LUMOS_EXPORT WeakReference : ReferenceBase
    {
    public:
        WeakReference(T* ptr)
        {
            m_Ptr = ptr;
            reference();
        }
        
        ~WeakReference()
        {
            if(unreference())
                delete m_Ptr;
        }
        
    private:
        T* m_Ptr;
    };
    
    template<class T>
    class LUMOS_EXPORT Owned : ReferenceBase
    {
    public:
        Owned(T* ptr)
        {
            m_Ptr = ptr;
            reference();
        }
        
        ~Owned()
        {
            if(unreference())
                delete m_Ptr;
        }
        
    private:
        T* m_Ptr;

    };
           
#define CUSTOM_SMART_PTR
#ifdef CUSTOM_SMART_PTR
    
    template<class T>
    using Ref = Reference<T>;
    
    template <typename T, typename ... Args>
    Ref<T> LUMOS_EXPORT CreateRef(Args&& ...args)
    {
        auto ptr = new T(args ...);
        
        return Reference<T>(ptr);
    }
    
    template <typename T>
    Ref<T> LUMOS_EXPORT CreateRef(T* t)
    {
        return Reference<T>(t);
    }
            
    template<class T>
    using Scope = std::unique_ptr<T>;
    
    template <typename T, typename ... Args>
    Scope<T> LUMOS_EXPORT CreateScope(Args&& ...args)
    {
        return std::make_unique<T>(args ...);
    }
    
    template <typename T>
    Scope<T> LUMOS_EXPORT CreateScope(T* t)
    {
        return std::unique_ptr<T>(t);
    }
#else
    template<class T>
    using Ref = std::shared_ptr<T>;
    
    template <typename T, typename ... Args>
    Ref<T> LUMOS_EXPORT CreateRef(Args&& ...args)
    {
        return std::make_shared<T>(args ...);
    }
    
    template <typename T>
    Ref<T> LUMOS_EXPORT CreateRef(T* t)
    {
        return std::shared_ptr<T>(t);
    }
            
    template<class T>
    using WeakRef = std::weak_ptr<T>;

    template<class T>
    using Scope = std::unique_ptr<T>;
    
    template <typename T, typename ... Args>
    Scope<T> LUMOS_EXPORT CreateScope(Args&& ...args)
    {
        return std::make_unique<T>(args ...);
    }
    
    template <typename T>
    Scope<T> LUMOS_EXPORT CreateScope(T* t)
    {
        return std::unique_ptr<T>(t);
    }
#endif
}
