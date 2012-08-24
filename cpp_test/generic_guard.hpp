#ifndef GENERIC_GUARD_HPP__
#define GENERIC_GUARD_HPP__

template <typename Func>
struct generic_guard
{
	Func func_;

	generic_guard( Func func )
		: func_(func)
	{}

	~generic_guard()
	{
		func_();
	}
};

template <typename Func>
generic_guard<Func> finally( Func func )
{
	return generic_guard<Func>(func);
}

//template <typename Pre, typename Post>
//struct generic_guard
//{
//	Pre pre_;
//	Post post_;
//
//	generic_guard( Pre pre, Post post )
//		: pre_(pre), post_(post)
//	{
//		pre_();
//	}
//
//	~generic_guard()
//	{
//		post_();
//	}
//};
//
//template <typename Pre, typename Post>
//generic_guard<Pre, Post> make_generic_guard( Pre pre, Post post )
//{
//	return generic_guard<Pre, Post>(pre, post);
//}


#endif // GENERIC_GUARD_HPP__
