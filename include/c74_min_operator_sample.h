/// @file	
///	@ingroup 	minapi
///	@copyright	Copyright (c) 2016, Cycling '74
///	@license	Usage of this file and its contents is governed by the MIT License

#pragma once

namespace c74 {
namespace min {


	/// The base class for all template specializations of #sample_operator.

	class sample_operator_base {};


	/// Inheriting from #sample_operator extends your class' functionality to processing audio
	/// by calculating samples one at a time using a member function named #calculate().
	///
	/// Your #calculate function must take the same number of parameters as the #input_count template arg.
	/// Your #calculate function must return an array of samples of the same size as the #output_count template arg.
	/// For example, if your object inherits from sample_operator<3,2> then your #calculate() will be prototypes as:
	/// @code
	/// samples<2> calculate(sample input1, sample input2, sample input3);
	/// @endcode
	///
	/// @tparam input_count		The number of audio inputs for your object.
	/// @tparam output_count	The number of audio outputs for your object.

	template<int input_count, int output_count>
	class sample_operator : public sample_operator_base {
	public:
		static constexpr size_t inputcount() {
			return m_inputcount;
		}

		static constexpr size_t outputcount() {
			return m_outputcount;
		}

	private:
		static constexpr size_t m_inputcount = input_count;
		static constexpr size_t m_outputcount = output_count;
	};


	// The min_performer class is used by the wrapper code to adapt the calls coming from the Max application
	// to the calculate() method implemented in the Min class.
	//
	// There are two versions of this.
	// This one is optimized for the most common case: a single input and a single output.
	// The other version is generic for N inputs and N outputs.

	template<class T>
	class min_performer<T, typename std::enable_if< std::is_base_of<c74::min::sample_operator<1,1>, T >::value>::type> {
	public:
		static void perform(minwrap<T>* self, max::t_object *dsp64, double **in_chans, long numins, double **out_chans, long numouts, long sampleframes, long flags, void *userparam) {
			auto in_samps = in_chans[0];
			auto out_samps = out_chans[0];
			
			for (auto i=0; i<sampleframes; ++i) {
				auto in = in_samps[i];
				auto out = self->obj.calculate(in);
				out_samps[i] = out;
			}
		}
	};


	// To implement the min_performer class generically we use std::array<sample> for both input and output.
	// However, we wish to define the #calculate() member function in the Min class with each sample as a
	// separate argument.
	// To make this translation efficiently and without out lots of duplicated code we use a pattern whereby
	// the sequence of indices for std::array are generated at compile time and then used to make the call
	// as a variadic template function.
	//
	// for more information, see:
	// http://stackoverflow.com/questions/16834851/passing-stdarray-as-arguments-of-template-variadic-function

	namespace detail {
		template<int... Is>
		struct seq { };
		
		template<int N, int... Is>
		struct gen_seq : gen_seq<N - 1, N - 1, Is...> { };
		
		template<int... Is>
		struct gen_seq<0, Is...> : seq<Is...> { };
	}


	template<class T, int count>
	struct callable_samples {

		callable_samples(minwrap<T>* a_self)
		: self (a_self)
		{}

		void set(size_t index, sample& value) {
			data[index] = value;
		}

		auto call() {
			return call(detail::gen_seq<count>());
		}

		template<int... Is>
		auto call(detail::seq<Is...>) {
			return self->obj.calculate(data[Is]...);
		}

		samples<count>	data;
		minwrap<T>*		self;
	};

	
	// perform_copy_output() copies the output sample(s) from a sample_operator's #calculate() function.
	// it does so in a way that the returned type can either be a single #sample or an array of #samples<N>
	
	template<class T, typename type_returned_from_calculate>
	void perform_copy_output(minwrap<T>* self, size_t index, double** out_chans, type_returned_from_calculate vals) {
		for (auto chan=0; chan < self->obj.outputcount(); ++chan)
			out_chans[chan][index] = vals[chan];
	}

	template<class T>
	void perform_copy_output(minwrap<T>* self, size_t index, double** out_chans, sample val) {
		out_chans[0][index] = val;
	}
	
	
	// The generic version of the min_performer class, for N inputs and N outputs.
	// See above for the 1x1 optimized version.

	template<class T>
	class min_performer<T, typename std::enable_if<
		std::is_base_of<c74::min::sample_operator_base, T >::value
		&& !std::is_base_of<c74::min::sample_operator<1,1>, T >::value
	>::type> {
	public:
		static void perform(minwrap<T>* self, max::t_object *dsp64, double **in_chans, long numins, double **out_chans, long numouts, long sampleframes, long flags, void *userparam) {
			for (auto i=0; i<sampleframes; ++i) {
				callable_samples<T, T::inputcount()> ins(self);

				for (auto chan=0; chan < self->obj.inputcount(); ++chan)
					ins.set(chan, in_chans[chan][i]);

				auto out = ins.call();

				perform_copy_output(self, i, out_chans, out);
			}
		}
	};

}} // namespace c74::min


// If you inherit from sample_operator then define this function to
// add audio support to the Max class.
// Will be called from #define_min_external()

template<class cpp_classname>
typename std::enable_if<std::is_base_of<c74::min::sample_operator_base, cpp_classname>::value>::type
define_min_external_audio(c74::max::t_class* c) {
	c74::max::class_addmethod(c, (c74::max::method)c74::min::min_dsp64<cpp_classname>, "dsp64", c74::max::A_CANT, 0);
	c74::max::class_dspinit(c);
}
