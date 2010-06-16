#ifndef __simtime_H
#define __simtime_H
/*
 * Copyright (c) 2010 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

# include  <stdint.h>

class simtime_t {

    public:
      simtime_t();
      simtime_t(uint64_t mant, int use_exp);

	// Add that time into this time
      simtime_t& operator += (const simtime_t&that);

      bool operator < (const simtime_t&that) const;

	// Get time in given units
      unsigned long long units_value(int use_units) const;

      uint64_t peek_mant() const { return mant_; }
      int      peek_exp()  const { return exp_; }

    private:
      uint64_t mant_;
      int exp_;
};

inline simtime_t::simtime_t()
{
      mant_ = 0;
      exp_  = 0;
}

inline simtime_t::simtime_t(uint64_t m, int e)
{
      mant_ = m;
      exp_  = e;
}

inline simtime_t& simtime_t::operator += (const simtime_t&that)
{
      int use_exp = that.exp_;
      uint64_t use_mant = that.mant_;

      while (use_exp < exp_) {
	    mant_ *= 10;
	    exp_ -= 1;
      }
      while (exp_ < use_exp) {
	    use_mant *= 10;
	    use_exp -= 1;
      }

      mant_ += use_mant;
}

inline bool simtime_t::operator < (const simtime_t&r) const
{
      int l_exp = exp_;
      int r_exp = r.exp_;

      unsigned long long l_mant = mant_;
      unsigned long long r_mant = r.mant_;

      while (l_exp > r_exp) {
	    l_exp -= 1;
	    l_mant *= 10ULL;
      }
      while (r_exp > l_exp) {
	    r_exp -= 1;
	    r_mant *= 10ULL;
      }
      return l_mant < r_mant;
}

inline unsigned long long simtime_t::units_value(int use_units) const
{
      unsigned long long use_time = mant_;
      int use_exp = exp_;

      while (use_exp < use_units) {
	    use_time += 5ULL;
	    use_time /= 10ULL;
	    use_exp += 1;
      }
      while (use_exp > use_units) {
	    use_time *= 10ULL;
	    use_exp -= 1;
      }

      return use_time;
}

#endif
