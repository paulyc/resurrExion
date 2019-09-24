template <typename Value_T, typename Container_Base_T, typename Container_T>
    class Stream
    {
    public:
        Stream(Container_T &container) : _container(container) {}

        void forEach(std::function<void(typename Container_T::value_type)> fun) {
            for (typename Container_T::iterator diffit = this->begin();
                 diffit != this->end(); ++diffit)
            {
                fun(*diffit);
            }
        }

        template <typename Mapped_Value_T>
        Stream<Mapped_Value_T, Container_Base_T, Container_Base_T<typename Mapped_Value_T>>
        map(std::function<Map_Result_T(typename Container_T::value_type)> fun) {
            for (typename Container_T::iterator diffit = this->begin();
                 diffit != this->end(); ++diffit)
            {
                return fun(*diffit);
            }
        }
    private:
        Container_T &_container;
    };

    template<typename Container_Base_T, typename Container_T>
    Stream<typename Container_T::value_type, Container_Base_T, Container_T> stream(Container_T &container)
    {
        return Stream<typename Container_T::value_type, Container_Base_T, Container_T>(container);
    }
