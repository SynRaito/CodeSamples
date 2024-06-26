#pragma once

#include "../../Runtime/Core/Core.h"
#include <vector>
#include <algorithm>
#include <functional>
#include "../../Runtime/Core/Object.h"
#include "../../Runtime/Engine/PTR.h"

namespace Syn
{
    template <typename... T>
    class SYN_API Event
    {
    private:
        std::vector<void(*)(T...)> functionReferences;

    public:
        size_t RefCount()
        {
            return functionReferences.size();
        }

        void Register(void (*ref)(T...))
        {
            functionReferences.push_back(ref);
        }

        void Unregister(void (*ref)(T...))
        {
            auto found = std::find(functionReferences.begin(), functionReferences.end(), ref);
            if (found != functionReferences.end())
            {
                functionReferences.erase(found);
            }
        }

        void Trigger(T... args)
        {
            for (auto ref : functionReferences)
            {
                ref(args...);
            }
        }

        Event<T...>& operator+(void (*ref)(T...))
        {
            Register(ref);

            return *this;
        }

        Event<T...>& operator-(void (*ref)(T...))
        {
            Unregister(ref);

            return *this;
        }

        Event<T...>& operator+=(void (*ref)(T...))
        {
            Register(ref);

            return *this;
        }

        Event<T...>& operator-=(void (*ref)(T...))
        {
            Unregister(ref);

            return *this;
        }
    };

    /***
     * T = Potential multiple parameters
    **/
    template <typename... T>
    class SYN_API LinkedEvent
    {
        template <typename... T>
        class SYN_API LinkedEventClass
        {
        public:
            Syn::Engine::PTR<Syn::Core::Object> objRef;
            void (Syn::Core::Object::*funcRef)(T...);

            void operator ()(T... args)
            {
                return (this->objRef.Get().*this->funcRef)(args...);
            }
        };

    private:
        std::vector<LinkedEventClass<T...>> functionReferences;

    public:
        inline size_t RefCount()
        {
            return functionReferences.size();
        }

        /**
         * Example Usage : Register(obj, DYNAMIC_LINKED_EVENT(&Syn::Core::Obj::TestFunc));
         * obj must be PTR
         **/
        void Register(Syn::Engine::PTR<Syn::Core::Object> Obj, void (Syn::Core::Object::*Func)(T...))
        {
            LinkedEventClass<T...> LinkedEventClassObj;
            LinkedEventClassObj.objRef = Obj;
            LinkedEventClassObj.funcRef = Func;

            functionReferences.push_back(LinkedEventClassObj);
        }

        void Unregister(Syn::Engine::PTR<Syn::Core::Object> Obj, void (Syn::Core::Object::*Func)(T...))
        {
            auto found = std::find_if(functionReferences.begin(), functionReferences.end(),
                                      [&](LinkedEventClass<T...> const& lec)
                                      {
                                          return lec.objRef == Obj && Func == lec.funcRef;
                                      });
            if (found != functionReferences.end())
            {
                functionReferences.erase(found);
            }
        }

        void Trigger(T... args)
        {
            for (auto& Ref : functionReferences)
            {
                if (Ref.objRef.IsValid())
                {
                    Ref(args...);
                }
            }
        }

        LinkedEvent<T...>& operator+(LinkedEventClass<T...> ClassObj)
        {
            Register(ClassObj->objRef, ClassObj->funcRef);
            return *this;
        }

        LinkedEvent<T...>& operator+=(LinkedEventClass<T...> ClassObj)
        {
            Register(ClassObj->objRef, ClassObj->funcRef);
            return *this;
        }
    };


    //Macros
#define DYNAMIC_LINKED_EVENT LinkedEvent<>
#define DYNAMIC_LINKED_EVENT_ONE_PARAM(Param1) LinkedEvent<Param1>
#define DYNAMIC_LINKED_EVENT_TWO_PARAM(Param1,Param2) LinkedEvent<Param1,Param2>
#define DYNAMIC_LINKED_EVENT_THREE_PARAM(Param1,Param2,Param3) LinkedEvent<Param1,Param2,Param3>
#define DYNAMIC_LINKED_EVENT_FOUR_PARAM(Param1,Param2,Param3,Param4) LinkedEvent<Param1,Param2,Param3,Param4>
#define DYNAMIC_LINKED_EVENT_FIVE_PARAM(Param1,Param2,Param3,Param4,Param5) LinkedEvent<Param1,Param2,Param3,Param4,Param5>

#define WRAP_LINKED_EVENT_FUNCTION(T) static_cast<void(Syn::Core::Object::*)()>(T)
#define WRAP_LINKED_EVENT_FUNCTION_ONE_PARAM(T, Param1) static_cast<void(Syn::Core::Object::*)(Param1)>(T)
#define WRAP_LINKED_EVENT_FUNCTION_TWO_PARAM(T, Param1, Param2) static_cast<void(Syn::Core::Object::*)(Param1,Param2)>(T)
#define WRAP_LINKED_EVENT_FUNCTION_THREE_PARAM(T, Param1, Param2, Param3) static_cast<void(Syn::Core::Object::*)(Param1,Param2,Param3)>(T)
#define WRAP_LINKED_EVENT_FUNCTION_FOUR_PARAM(T, Param1, Param2, Param3, Param4) static_cast<void(Syn::Core::Object::*)(Param1,Param2,Param3,Param4)>(T)
#define WRAP_LINKED_EVENT_FUNCTION_FIVE_PARAM(T, Param1, Param2, Param3, Param4,Param5) static_cast<void(Syn::Core::Object::*)(Param1,Param2,Param3,Param4,Param5)>(T)
}
