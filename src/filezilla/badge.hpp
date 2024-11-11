#ifndef FZ_BADGE_HPP
#define FZ_BADGE_HPP

// Freely inspired to https://awesomekling.github.io/Serenity-C++-patterns-The-Badge/

namespace fz
{

template <typename T>
class badge
{
	friend T;
	badge(){}
};

}

#endif // FZ_BADGE_HPP
