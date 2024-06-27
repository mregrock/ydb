#pragma once
#include "defs.h"
#include <algorithm>
#include "hullds_ut.h"
#include "hullds_generic_it.h"

template<typename T>
concept HasNext = requires(T t) { t.Next(); };

template<typename T>
concept HasPrev = requires(T t) { t.Prev(); };

template<typename T>
concept HasSeekToFirst = requires(T t) { t.SeekToFirst(); };

namespace NKikimr {

    template <typename TKey, typename TMemRec, typename TComp> 
    class THeapIterator {
    protected:
        struct THeapItem {
            TKey Key;
            std::variant<TVectorIt*, TGenericNWayBackwardIterator<TKey, TMemRec>*> Iter;

            friend bool operator<(const THeapItem& x, const THeapItem& y) { 
                return TComp()(x.Key, y.Key);
            } 

            template <class TIter>
            THeapItem(TIter *iter){
                Iter = iter;
            }
        };

        std::vector<THeapItem> Heap;
        size_t HeapItems = 0;

    public:
        bool Valid() const {
            return HeapItems;
        }

        template <class TIter> 
        void Add(TIter* iter) {
            THeapItem item(iter);
            Heap.push_back(item);
            HeapItems++;
            std::push_heap(Heap.begin(), Heap.begin() + HeapItems);
        }

        template<typename TMerger>
        void PutToMergerAndAdvance(TMerger *merger) {
            Y_ABORT_UNLESS(Valid());
            const TKey key = Heap.front().Key;
            while (HeapItems && Heap.front().Key == key) {
                std::pop_heap(Heap.begin(), Heap.begin() + HeapItems);
                THeapItem& item = Heap[HeapItems - 1];
                const bool remainsValid = std::visit([&](auto *iter) {
                    if constexpr (!HasNext<decltype(*iter)>){
                        return false;
                    }
                    else{
                        iter->PutToMerger(merger);
                        iter->Next();
                        if (iter->Valid()) {
                            item.Key = iter->GetCurKey();
                            return true;
                        } else {
                            return false;
                        }
                    }
                }, item.Iter);
                if (remainsValid) {
                    std::push_heap(Heap.begin(), Heap.begin() + HeapItems);
                } else {
                    --HeapItems;
                }
            }
        }

        void Next(){
            Y_ABORT_UNLESS(Valid());
            const TKey key = Heap.front().Key;
            while (HeapItems && Heap.front().Key == key) {
                std::pop_heap(Heap.begin(), Heap.begin() + HeapItems);
                THeapItem& item = Heap[HeapItems - 1];
                const bool remainsValid = std::visit([&](auto *iter) {
                    if constexpr (!HasNext<decltype(*iter)>){
                        return false;
                    }
                    else{
                        iter->Next();
                        if (iter->Valid()) {
                            item.Key = iter->GetCurKey();
                            return true;
                        } else {
                            return false;
                        }
                    }
                }, item.Iter);
                if (remainsValid) {
                    std::push_heap(Heap.begin(), Heap.begin() + HeapItems);
                } else {
                    --HeapItems;
                }
            }
        }

        void Prev(){
            Y_ABORT_UNLESS(Valid());
            const TKey key = Heap.front().Key;
            while (HeapItems && Heap.front().Key == key) {
                std::pop_heap(Heap.begin(), Heap.begin() + HeapItems);
                THeapItem& item = Heap[HeapItems - 1];
                const bool remainsValid = std::visit([&](auto *iter) {
                    if constexpr (!HasPrev<decltype(*iter)>){
                        return false;
                    }
                    else{
                        iter->Prev();
                        if (iter->Valid()) {
                            item.Key = iter->GetCurKey();
                            return true;
                        } else {
                            return false;
                        }
                    }
                }, item.Iter);
                if (remainsValid) {
                    std::push_heap(Heap.begin(), Heap.begin() + HeapItems);
                } else {
                    --HeapItems;
                }
            }
        }

        void Seek(const TKey &key) {
            auto pivot = [&](THeapItem& item){
                return std::visit([&] (auto *iter){
                    iter->Seek(key);
                    if (iter->Valid()){
                        item.Key = iter->GetCurKey();
                        return true;
                    }
                    return false;
                }, item.Iter); 
            }; 
            auto it = std::partition(Heap.begin(), Heap.end(), pivot); 
            std::make_heap(Heap.begin(), it);
            HeapItems = it - Heap.begin();
        }

        void SeekToFirst() {
            auto pivot = [&](THeapItem& item){
                return std::visit([&] (auto *iter){
                    if constexpr (!HasSeekToFirst<decltype(*iter)>){
                        return false;
                    }
                    else{
                        iter->SeekToFirst();
                        if (iter->Valid()){
                            item.Key = iter->GetCurKey();
                            return true;
                        }
                        return false;
                    }
                }, item.Iter); 
            }; 
            auto it = std::partition(Heap.begin(), Heap.end(), pivot); 
            std::make_heap(Heap.begin(), it);
            HeapItems = it - Heap.begin();
        }

        template <class TMerger>
        void WalkForward(TKey key, TMerger merger, std::function<bool(const TKey&, TMerger)> callback){
            for (int idx = 0; idx < HeapItems; idx++){
                auto *iter = Heap[idx];
                iter->Seek(key);
                while (iter->Valid()){
                    iter->PutToMerger(&merger);
                    if (!callback(iter->GetCurKey(), merger)){
                        break;
                    }
                    merger.Clear();
                    iter->Next();
                }
            }
        }

        template <class TMerger>
        void WalkBackward(TKey key, TMerger merger, std::function<bool(const TKey&, TMerger)> callback){
            for (int idx = 0; idx < HeapItems; idx++){
                auto *iter = Heap[idx];
                iter->Seek(key);
                while (iter->Valid()){
                    iter->PutToMerger(&merger);
                    if (!callback(iter->GetCurKey(), merger)){
                        break;
                    }
                    merger.Clear();
                    iter->Prev();
                }
            }
        }

        TKey GetCurKey() const {
            Y_DEBUG_ABORT_UNLESS(Valid());
            return Heap.front().Key;
        }
    };
    
} // NKikimr