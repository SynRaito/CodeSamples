#pragma once

#include "../Core/Core.h"
#include "../Engine/ObjectGC.h"
#include "../Core/GameObject.h"
#include "ObjectGC.h"
#include <memory>

namespace Syn
{
	namespace Core
	{
		class GameInstance;
	}
}

namespace Syn::Engine {
	// Don't pass by reference
	template<class T>
	class SYN_API PTR : public IGarbageCollectable {
		friend class Syn::Core::GameInstance;

		template<class U>
		friend class PTR;

	private:

		std::shared_ptr<T> rawPtr;
		bool* isValid = nullptr;

	private:

		void* operator new(size_t size){
			void *p = ::operator new(size); 
			return p;
		}
		void operator delete(void*p){
			free(p);
		}

	public:

		PTR()
		{
			if (rawPtr.use_count() == 0)
			{
				rawPtr = std::make_shared<T>();
				isValid = new bool;
				*isValid = true;
			}
		}

		~PTR() {
			if (rawPtr.use_count() == 2) {
				*isValid = false;
			}
		}

		operator PTR<Syn::Core::Object>() const
		{
			PTR<Syn::Core::Object> ptr;
			ptr.rawPtr = rawPtr;
			ptr.isValid = isValid;
			return ptr;
		}

		operator PTR<Syn::Core::GameObject>() const
		{
			PTR<Syn::Core::GameObject> ptr;
			ptr.rawPtr = rawPtr;
			ptr.isValid = isValid;
			return ptr;
		}

		T* operator->()
		{
			return rawPtr.get();
		}

		bool operator == (const PTR<T>& ptr)
		{
			if (rawPtr.get() == ptr.rawPtr.get())
				return true;
			return false;
		}

		T& Get() {
			return *rawPtr.get();
		}

		template<typename ... X>
		std::function<void(X...)> Bind(void(T::*ref)(X ...)) {
			return std::bind(ref, *rawPtr);
		}

		void Destroy() {
			rawPtr.reset();
			*isValid = false;
		}

		bool IsValid() {
			return isValid;
		}

		bool IsCollectable() override
		{
			if (rawPtr.use_count() == 1) {
				return true;
			}
			return false;
		}
	};
}
