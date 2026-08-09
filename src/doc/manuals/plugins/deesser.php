<?php
	plugin_header();
	$sc     =   (strpos($PAGE, 'sc_') === 0);
	$m      =   (strpos($PAGE, '_mono') > 0) ? 'm' : 's';
	$cc     =   ($m == 'm') ? 'mono' : 'stereo';
	$tt     =   ($m == 'ms') || ($m == 'lr');
?>

<p>
	This plugin allows to reduce hissing and whistling sounds in the audio. Additional equalizer allows to
	raise sensitivity for specific frequencies and cut off low and high frequencies which should not give any impact.
	Flexible sidechain-control configuration <?php
		if ($sc)
			echo " and additional sidechain input" . (($m == 'm') ? '' : 's') . " are ";
		else
			echo " is";
	?> provided.
</p>
<p>Key features:</p>
<ul>
<li>Frequency split mode is provided which allows to separate high frequencies and keep low frequencies untouched.</li>
<li>Sidechain pre-equalization which allows to raise sensitivity for some frequency ranges.</li>
<?php if ($m == 's') { ?>
<li>Stereo split mode which allows to process left and right channels independently and then link the result together.</li>
<?php } ?>
</ul>

<p><b>Controls:</b></p>
<ul>
	<li><b>Split (mode)</b> - enables spliting of the signal into low-frequency and high frequency domain. Available opions:</li>
	<ul>
		<li><b>Off</b> - frequency splitting is disabled.</li>
		<li><b>Classic</b> - classic operating mode using IIR filters and allpass filters to compensate phase shifts.</li>
		<li><b>Modern</b> - modern operating mode using IIR shelving filters to adjust the gain of each frequency band.</li>
		<li><b>Linear Phase</b> - linear phase operating mode using FFT transform (FIR filters) to split signal into multiple bands, introduces additional latency.</li>
	</ul>
	<li><b>Split (slope)</b> - allows to configure the slope of Linkwitz-Riley filters for classic mode or similar filters of modern and linear phase mode.</li>
	<?php if ($m == 's') { ?>	
		<li><b>Stereo split</b> - Enables separate processing of the left and right channel.</li>
	<?php } ?>
	<li><b>Link</b> - the shared memory link is used to receive sidechain signal.</li>
	<li><b>Zoom</b> - zoom fader, allows to adjust zoom on the frequency chart.</li>
	<li><b>Pre-mix</b> - shows pre-mix control overlay.</li>
	<li><b>Sidechain</b> - shows sidechain control overlay.</li>
</ul>

<p><b>'Signal' section:</b></p>
<ul>
	<li><b>Input</b> - overall input gain.</li>
	<li><b>Output</b> - overall output gain.</li>
</ul>
<p><b>'Analysis' section:</b></p>
<ul>
	<li><b>FFT In</b> - enable analysis of the input signal.</li>
	<li><b>FFT Sc</b> - enable analysis of the sidechain signal.</li>
	<li><b>FFT Out</b> - enable analysis of the output signal.</li>
	<li><b>Reactivity</b> - the reactivity (smoothness) of the spectral analysis.</li>
	<li><b>Shift</b> - allows to adjust the overall gain of the analysis.</li>
</ul>

<p><b>'Pre-equalization' section:</b></p>
<ul>
	<li><b>HPF</b> combo - allows to select the high-pass filter slope or disable high-pass filter for the sidechain.</li>
	<li><b>HPF Freq</b> - the cut-off frequency of the high-pass filter.</li>
	<li><b>HPF Q</b> - the Q factor of the high-pass filter.</li>
	<li><b>Peak 1</b> button - enables first additional peak filter.</li>
	<li><b>Peak 1 Freq</b> - the frequency of first additional peak filter.</li>
	<li><b>Peak 1 Q</b> - the Q factor of first additional peak filter.</li>
	<li><b>Peak 1 Gain</b> - the amplification of first additional peak filter.</li>
	<li><b>Peak 2</b> button - enables second additional peak filter.</li>
	<li><b>Peak 2 Freq</b> - the frequency of second additional peak filter.</li>
	<li><b>Peak 2 Q</b> - the Q factor of second additional peak filter.</li>
	<li><b>Peak 2 Gain</b> - the amplification of second additional peak filter.</li>
	<li><b>LPF</b> combo - allows to select the low-pass filter slope or disable low-pass filter for the sidechain.</li>
	<li><b>LPF Freq</b> - the cut-off frequency of the low-pass filter.</li>
	<li><b>LPF Q</b> - the Q factor of the low-pass filter.</li>
</ul>

<p><b>'Deesser' section:</b></p>
<ul>
	<li><b>Ratio</b> - the signal reduction ratio of the deesser.</li>
	<li><b>Knee</b> - the size of the transition part (knee) of the deesser.</li>
	<li><b>Thresh</b> - the threshold of the deesser, placed in the middle of the knee.</li>
	<li><b>Attack</b> - the attack time of the deesser.</li>
	<li><b>Release</b> - the release time of the deesser.</li>
	<li><b>Hold</b> - the time period the envelope signal of the deesser holds it's maximum value before starting the release.</li>
	<li><b>Split</b> - the split frequency between high-frequeny band and low-frequency band.</li>
	<li><b>LF Link</b> - the amount of high gain reduction on the high-frequency band affects the low-frequency band.</li>
	<?php if ($m == 's') { ?>	
		<li><b>Link</b> - the link between the separately processed right and left audio channels.</li>
	<?php } ?>
</ul>

<p><b>Pre-mix control overlay:</b></p>
<ul>
	<?php if ($sc) { ?>
	<li><b>In -> SC</b> - the amount of signal from input channel added to the Sidechain.</li>
	<?php } ?>
	<li><b>In -> Link</b> - the amount of signal from input channel added to the shared memory link.</li>
	<?php if ($sc) { ?>
	<li><b>SC -> In</b> - the amount of signal from sidechain input channel added to the input channel.</li>
	<li><b>SC -> Link</b> - the amount of signal from sidechain input channel added to the shared memory link.</li>
	<?php } ?>
	<li><b>Link -> In</b> - the amount of signal from shared memory link added to the input channel.</li>
	<li><b>Link -> SC</b> - the amount of signal from shared memory link added to the sidechain channel.</li>
</ul>

<p><b>Sidechain control overlay:</b></p>
<ul>
	<li><b>Position</b> - the position of the sidechain input. Available variants:</li>
	<li><b>Preamp</b> - pre-amplification of the sidechain signal.</li>
	<li><b>Reactivity</b> - reactivity of the sidechain signal.</li>
	<li><b>Lookahead</b> - look-ahead time of the sidechain relative to the input signal.</li>
	<li><b>Listen</b> - allows to listen the <b>processed</b> sidechain signal.</li>
	<li><b>Setup</b> - combo boxes that allow to switch different settings for sidechain processing. Available types are:</li>
	<ul>
		<li><b>Internal</b> - the audio inputs of plugin are used as sidechain signal.</li>
		<?php if ($sc) { ?>
			<li><b>External</b> - additional sidechain audio inputs of plugins are used as sidechain signal.</li>
		<?php } ?>
		<li><b>Link</b> - the shared memory link is used to receive sidechain signal.</li>
		<li><b>Peak</b> - peak mode.</li>
		<li><b>RMS</b> - Root Mean Square (RMS) of the input signal.</li>
		<li><b>LPF</b> - input signal processed by recursive 1-pole Low-Pass Filter (LPF).</li>
		<li><b>SMA</b> - input signal processed by Simple Moving Average (SMA) filter.</li>
	</ul>
	
	<?php if ($m != 'm') { ?>
	<li><b>Stereo split</b> - Enables separate processing of the left and right channel.</li>
	<li><b>Source</b> - combo boxes that allow to switch sidechain signal for stereo channel or individual left and right channels:</li>
	<ul>
		<li><b>Middle</b> - middle part of signal is used for sidechain processing.</li>
		<li><b>Side</b> - side part of signal is used for sidechain processing.</li>
		<li><b>Left</b> - only left channel is used for sidechain processing.</li>
		<li><b>Right</b> - only right channel is used for sidechain processing.</li>
		<li><b>Min</b> - the absolute minimum value is taken from stereo input.</li>
		<li><b>Max</b> - the absolute maximum value is taken from stereo input.</li>
	</ul>
	<?php } ?>
</ul>

