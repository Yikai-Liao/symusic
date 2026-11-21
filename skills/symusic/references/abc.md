# ABC notation

Use `Score.from_abc(text)` for a string or `Score(path)` for a file. The reader expands the first tune's parts and repeats into a score at 480 ticks per quarter. Use `ttype` to request times in quarter notes or seconds.

Symusic uses ABC 2.1 playback rules for every tune, including older and unversioned input. The version declaration controls syntax checks. `abc2midi` is the reference for conventions such as the one-tick initial note offset and extensions such as chord accompaniment. Backing instruments and program changes can produce separate tracks.

A closing `:|` without a matching `|:` repeats from the latest double bar or end of a repeated section, or from the beginning if neither exists. An explicit `|:` takes precedence over intervening double bars. At segno, coda, Da Capo and Fine marks, playback continues in written order.

On entering a reordered part or skipping an ending, notes use the unit length and key signature at their written position. A part label inside a bar also preserves accidentals carried from earlier notes in that bar. Measure rests and implicit tuplet ratios use the written meter. A `K:` change adds timed key metadata when playback visits that field. As in abc2midi, closing an explicit overlay that splits into multiple lanes clears the accidentals carried within it.

Broken rhythm and tuplet ratios apply to written notes across part labels. Each note keeps its adjusted length when a part jump or repeat plays only part of the group. Timed fields and dynamics between the two notes of a broken pair take effect at the adjusted note boundary. A grace group can borrow from a host in the next written part when playback continues across the label. Skipping the grace group leaves that duration with the host.

Import drops single-note retuning from `%%MIDI snt`, which abc2midi writes as SysEx. An invalid key or pitch still warns. Import also drops copyright text from `Z:abc-copyright` and `%%abc-copyright`, and silence after the last event. `Score.end()` ends at the last retained event.

When reading a file, `I:abc-include defaults.abh` or `%%abc-include defaults.abh` imports that header at the point of the instruction. Includes can be nested, and every name is relative to the score's directory. Unavailable files and cycles warn and are skipped. `Score.from_abc(text)` does not open referenced files.

ABC text uses UTF-8 by default. A file header can declare `I:abc-charset us-ascii`. Use ABC text escapes for characters outside ASCII in such a file. Each included header can declare its own charset. Convert files encoded with an ISO-8859 charset to UTF-8 before import.

`R:hornpipe` enables swing and sets a 2:1 broken-rhythm ratio. A later `%%MIDI ratio` can change that ratio; a later `R:hornpipe` resets it to 2:1. These settings follow written order across voices and apply when the second note of a pair is read. Repeats and part order replay the written timing without carrying the setting backward. As in `abc2midi`, a hornpipe opening in 4/4 swings eighth notes and one opening in 2/4 swings sixteenth notes. A later `M:` changes the meter without choosing a new swing subdivision. Free meter uses the 4/4 subdivision without imposing a measure length.

In `%%MIDI beat a b c n`, `n` is the spacing between strong beats, in meter beats. A value of `0` uses the current meter's numerator. A written `M:` field restores the meter's default accent spacing without changing the three velocities or an explicit `%%MIDI beatstring`. Returning to an earlier meter on a repeat jump does not reset the spacing.

Backing patterns and the downbeat at a repeat bar follow the last performed `M:` field. A meter change preserves the beats that have already elapsed in the bar.

Additive meters such as `M:(2+3+2)/8` accent each group's first beat. The time signature keeps the total, here `7/8`. Free meter (`M:none`) gives the score a `4/4` time signature, as in abc2midi's MIDI output.

`Z` and `X` rest for one full measure by default. Use a positive integer to rest for more measures. `Z2` and `X2` leave the melody silent for two measures but preserve both measures of accompaniment and both bar boundaries for lyric and symbol alignment.

Clef suffixes `+8` and `-8` shift playback by an octave independently of `octave=`. Selecting a clef without a suffix clears the clef shift but leaves the `octave=` setting in effect. A `K:` field containing only clef or transposition options does not change the key signature or clear accidentals carried within the measure. With `I:propagate-accidentals octave`, the carried accidental applies to the octave after both shifts.

`transpose=` in `V:` or `K:` sets the selected voice's playback transposition. A closing header `K:` sets the opening key signature for the whole tune. Its `transpose=`, `octave=` and clef options apply to the selected header voice or, if none is selected, set the opening defaults. `%%MIDI transpose` adds a separate MIDI shift.

`%%MIDI transpose n` sets the MIDI shift in semitones, and `%%MIDI rtranspose n` adds to it. The shift follows abc2midi's track order. The conductor and each generated track replay their written commands and pass their final shift to the next track. A relative command can therefore apply more than once. Use `K:` or `V:` with `transpose=` for a voice-specific shift.

An inline `V:` selects a voice without breaking the rhythm of that voice. A numeric ID such as `01` identifies the same voice as `1`. Text IDs are distinguished by their first 20 characters. Pitch options on `V:` apply to the following note. When `V:` occurs between a grace group and its main note, only the main note uses the new pitch options.

A `Q:` tempo can combine up to four beat lengths. `Q:1/4=120` specifies 120 quarter notes per minute regardless of `L:`. The deprecated forms `Q:120` and `Q:C=120` use the current unit note length and warn when the input declares 2.1 or later.

## Lyrics, symbols and text

A run of `w:` fields applies to the written notes since the previous run in the same voice, even across music lines. Each note, chord or tied continuation has one slot. Grace notes, rests, spacers and extra attacks from ornaments take none. Adjacent `w:` fields supply alternative verses that cycle on successive visits to a part. An empty verse leaves the passage unsung. In `w: 1.~Three blind mice`, `Three` goes on the first note.

When a repeat jumps back into a music line, syllables already sung in the current part visit are omitted until playback enters another line. Syllables on newly reached notes, including a later ending, keep their written alignment.

Titles, voice names and lyrics accept ABC text escapes such as `\'e`, `&eacute;` and `\u00e9`. An escape stays within its syllable, so `\u002a` produces a literal asterisk. Font switches `$0` through `$4` are skipped. `$$` represents a dollar sign.

Use `s:` to place chord symbols, annotations, or `!decorations!` on earlier written notes in the current voice. A `*` skips one note, `|` advances to the next bar, and adjacent `s:` lines can stack symbols on the same notes. Grace notes, rests, and spacers do not take symbol slots. The symbols have the same playback effects as the inline forms.

`U:` definitions apply from their written position across voices. Repeats, alternate endings and part order play the shorthand expansion that each note was written with.

`&&` plays the next two bar-delimited measures alongside the two just completed. Use ordinary bars inside the overlay. A repeat or section may close at its final bar. Lyrics and symbols follow the written order of the notes in both lanes.

A trailing backslash continues the music across comments, fields, and directives. The continued music keeps one line for lyric alignment. If a tempo field occurs before a continued chord closes, the new tempo applies to the whole chord.

An inline `[r:...]` remark does not interrupt a note, chord, grace group, or macro. For example, `C[r:remark]2` plays as `C2`. A remark-like string inside quoted text is preserved.

## Chords and articulation

The first member of a chord sets the time of the next event. Each member keeps its own duration, and a suffix after the closing bracket multiplies them all. In `[C2E3]2 D`, D starts after four units and E has a written length of six. If a tied member overlaps its continuation, one attack holds through the later release.

`%%MIDI trim` and `%%MIDI expand` change the sounding length of ordinary melodic notes. A tie keeps its release at the end of the joined note. With `L:1/4`, the tied C in `C-C D z` lasts 959 ticks. The following D lasts 419 ticks with `%%MIDI trim 1/8` or 539 ticks with `%%MIDI expand 1/8`.

The channel used for the attack determines the release. On MIDI channel 10, a percussion note keeps its performed length under `trim`, `expand` and stress model 1. The D in the same example lasts 479 ticks under either directive. Stress can still change its velocity. In a chord split across channels, each member uses its own attack channel.

The default spread delays successive chord attacks by 10 ticks, including the second D in `[DD]`. The spread can shift releases still pending under `%%MIDI expand`. The next written event starts on time. Use `%%MIDI chordattack 0` for simultaneous attacks.

Chord symbols accept equivalent qualities such as `Amin7` and `Am7`, or `A+7` and `Aaug7`. An exact `%%MIDI chordname` definition takes precedence over the built-in spelling. In `"C9/D"`, D is the lowest tone of the chord; in `"C9/A"`, A is a separate bass note below the chord. Roots and slash basses accept doubled ASCII accidentals as in `"Cbb"` and `"D/C##"`. Accidentals can cross the octave boundary without changing register: `Cb` sounds below C, and `B#` sounds above B. A parenthesized alternate, as in `"C9/D(Em)"`, does not change playback.

`%%MIDI chordname` registers a name for every voice from that point in the written tune. A custom name needs one to seven bytes and one to ten semitone intervals. Once a symbol is recognized, it uses the final registered voicing throughout playback, including on repeats. As in `abc2midi`, redefining the latest newly registered name once leaves its first definition in effect. Define each name once for predictable playback across players.

An unrecognized quality produces a warning. A voice with a recognized chord symbol plays unknown qualities as major chords on the written roots. A voice needs at least one recognized chord symbol to create chord backing.

Inside a chord, a dot shortens the following member. Before the opening bracket it shortens the whole chord. Inner dynamics take effect at the marked member and carry forward in written order. `!tenuto!` gives the marked note, chord or member its full duration, overriding `%%MIDI trim` and `%%MIDI expand`. `!invertedfermata!` plays the same hold as `!fermata!`. Fingering marks such as `!1!` are for display.

`%%MIDI randomchordattack n` sets a random gap of zero to `n-1` ticks. Set `n` to zero to restore the fixed gap. `!arpeggio!` triples the gap or range for the marked chord. A `y` spacer or barline consumes note ornaments attached to it. Members starting a tie inherit the current gap. Symusic limits delays for short members to keep their written order and release times. Random gaps are repeatable but can differ from abc2midi's samples.

`%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8` divides each measure into four equal written segments and gives each a MIDI velocity and a duration multiplier. The default model, `%%MIDI stressmodel 2`, stretches or compresses time within each measure, scaling all multipliers together so the full measure keeps its length. Model `1` instead sets the velocity and shortens gates without moving attacks; it leaves long notes outside its articulation window alone. The first profile supplies the tune default; subsequent body changes affect only later written notes. Put changes at a barline to keep the measure length unchanged. `%%MIDI beataccents` restores ordinary velocities and articulation without undoing model 2 timing. In a file, `%%MIDI ptstress "profile.txt"` reads the same numbers from a text file under the score's directory. String input does not open files. Invalid settings warn and keep the previous valid setting.

A fermata adds time before either stress model is applied. The added time shifts the stress position of later notes until the next written barline. Proportional grace notes borrow from the lengthened duration. If a playback change falls between a grace group and its host, the group borrows from the original duration to leave room for the host.

Before a chord, grace notes subtract the time they borrow from each host member's duration. Within a grace group, the first member of a chord sets its advance.

Staccato places the release halfway through the unwarped note. Model 1 calculates its articulation on that sounding interval. Model 2 maps the release and the full span separately, so adding a dot does not move later attacks. A dot inside a chord affects only its marked member.

Model 1 also samples stress separately for each attack in a trill or `R` roll. Model 2 stretches the host first, then uses its velocity for each generated attack without changing the velocities of later notes. `%%MIDI expand` and `%%MIDI trim 0/1` turn off stress shortening but keep stress velocities.

In model 1, a tied note that outlasts the stress window uses the ordinary beat velocity of its first attack. One-shot volume commands still apply at that attack, and a tied continuation never creates a new attack or shortens the gate just to apply stress.

ABC 2.1 section 4.14 gives each voice an opening dynamic of `!mf!`. Symusic applies it to every input version. With the default accents in 4/4, a note has velocity 90 on the downbeat, 80 on the other beats and 65 between beats. For matching velocities in `abc2midi`, write `!mf!` explicitly or give both readers the same `%%MIDI beat` settings.

`!pppp!` and `!ffff!` are equivalent to `!ppp!` and `!fff!`. An accent raises the marked attack by one dynamic step, up to 127. An accent before a chord applies to the whole chord. An accent inside applies only to its member. `%%MIDI vol` sets the exact velocity.

As in abc2midi, the start and end of a crescendo or diminuendo each change the beat velocities by a fixed step. `%%MIDI deltaloudness n` changes the step from its default of 15 for marks written later, across voices. A repeat applies each mark's original step again without resetting the current velocity.

Within a grace group, `{a>b}` has the same relative lengths as `{a3/2b/2}`. This does not change the broken rhythm of the surrounding notes: `A<{a>b}A` still divides the two main notes in a 1:3 ratio.

Fixed grace-note lengths use the tuplet ratio active at the opening brace. For `(3C{d}EF`, the grace borrows time from `E` and counts separately from the three tuplet notes. Proportional grace uses the performed host length after tuplet scaling.

An ordinary `~` cut borrows time from its host for the upper neighbor. When the host spans three `L:` units after tuplets and fermata, `~` divides it into three attacks with upper and lower grace notes before the last two. For example, at `L:1/8`, `~D3` plays like `D {E}D {C}D`. An explicit grace group borrows from the first attack, and a tie to the next note starts at the last attack. `%%MIDI grace` controls the share of each host used by its grace notes. `%%MIDI expand` can extend grace releases into the host. `%%MIDI trim` applies to the main attacks.

A trill or `R` roll at the start of a tie spans the full tied duration. Each pulse uses the articulation and channel of the opening attack, even if the channel changes during the tie. A trill or roll written on a continuation adds no attacks.

Use `m: ~G3 = G{A}G{F}G` for a static playback macro, or `m: ~n3 = n{o}n{m}n` to apply that pattern to any unaltered note written with length `3`. In the replacement, `n` is the written pitch, `o` is one scale step above it, and `m` is one below. Use spaces on both sides of the separator when the target contains a natural sign, as in `m: F=G = CD`. Macro definitions can be inherited from the file header or changed in written order within the tune. Macro expansion precedes `U:` shorthand replacement. Export writes the performed notes, not the original macro definitions.

## MIDI directives and backing tracks

Use `%%MIDIdef name command` to name a MIDI command and `%%MIDIx name …` to apply one or more previously defined commands inside a tune. Only percent-form definitions are recognized in the file header. The first valid definition of a name wins. An undefined name stops the expansion with a warning; commands before it still apply. A single `%%MIDIx` use combines its controller curves instead of replacing each one with the next. Use `%%MIDI programbase 1` inside a tune to number programs from 1 instead of 0. The last setting inside the tune applies to all voices and backing instruments.

Use `%%MIDI voice Bass instrument=59` to set the Bass voice to GM tuba, or omit `Bass` to set the current voice. The standard's `instrument=` and `bank=` are always one-based, regardless of `programbase`. A non-default bank is stored as Bank Select controls on the resulting track. Add `mute` to silence the voice's note attacks from that point, including overlays, without changing lyric alignment.

`%%MIDI chordprog` uses one-based General MIDI numbers by default. `%%MIDI chordprog 20` selects Church Organ. An explicit `%%MIDI programbase` overrides that numbering.

Chord symbols and `droneon` apply to the melody or overlay lane where they appear. Their backing tracks follow that lane in the score. When a lane goes silent between overlays, its backing continues until switched off or the lane ends.

The drone reuses the MIDI delay of its initial Program Change for each of its two attacks, as in abc2midi. A drone that begins after a rest can start substantially later than the written command. End a drone with `%%MIDI droneoff` to emit its releases explicitly.

`%%MIDI control`, `portamento`, `noportamento`, and `pitchbend` accept an optional `bass` or `chord` target before their values. The target is a generated backing part of the current lane, not a named `V:` voice. Symusic writes the effect only to that part at the directive's performed time. abc2midi can also send these effects to the melody, time backing controllers earlier, or omit backing pitch bends.

Each melody or overlay lane can declare its own drum part with `%%MIDI drumon` or `[I:MIDI drumon]`. The part follows that lane's timing and appears after it in the score. `drumoff` switches off that lane without affecting the other parts. The older `!drum!` and `!nodrum!` marks are unsupported and produce an ignored-decoration warning.

`%%MIDI drummap C 35` maps the written pitch `C` to percussion pitch 35. Before `X:` it sets a file default. Inside the tune it changes the map when the instruction is reached on its track. A separate conductor pass can leave a map change from the first voice in effect before its first audible note. Write the initial mapping inside the tune header to reapply it at the start of that voice. Each overlay starts with the map left by preceding tracks. A lane’s drum backing starts with its melody’s final mapping for each pitch, then replays the lane’s map changes at their performed times.

In a key field such as `K:Dm exp _A`, `exp` leaves the D minor key signature in the score but replaces its implied accidentals with only those written in the field. Fractional accidentals in notes and key signatures produce MIDI pitch bends. Bends are shared by notes on a track: if overlapping notes require different bends, the reader warns and retains the channel's bend changes. Tuning applies in written order across voices and repeats. Use `%%temperament` for pitch-class offsets, or `%%MIDI temperamentlinear`, `%%MIDI temperamentequal`, and `%%MIDI temperamentnormal` to set the temperament. Inside the tune, `%%MIDI tuningsystem comma53` additionally quantizes the resulting pitches to a 53-step octave using abc2midi's mapping. A later temperament command does not turn this quantization off.

`%%MIDI makechordchannels n` assigns the next `n` ordinary chord attacks after the first to additional melodic routes. The first attack, notes outside a chord, and any excess chord attacks use the channel selected when the routes were allocated. A tied attack inherits the current route without consuming a slot; a tie continuation extends the original attack. Channel 10 applies the drum map rather than sounding transposition. The additional melodic routes remain subject to transposition even when the base route is percussion.

Every execution of `makechordchannels`, including on another repeat pass, allocates fresh channels. Each plays the program last declared on the selected channel (default 0). A copied program affects how the new channel sounds. An explicit program change on that channel sets what its own later allocations inherit. A zero or negative count leaves the allocation unchanged. Counts above nine use nine. Channel numbers are allocated in MIDI track order across voices, overlay lanes and backing tracks. `%%MIDI program channel program` addresses that numbered channel in the current lane. The reader reports an error if a chord allocation runs out of channels.

Use `!bend!` with `%%MIDI bendstring`, `bendstringex`, or `bendvelocity` to bend the next attack. A mark before a grace group affects the first grace note; a mark after the group affects the main note. MIDI commands between the group and its host run after the grace attacks. Use `!shape!` to apply both a configured bend and the controller curves from `%%MIDI controlstring`. Each controlstring starts with a controller number followed by one or more values. It replaces the previous curves unless preceded by `%%MIDI controlcombo`; a shape can combine up to three controller curves.

An envelope spans the note's gate, including tied continuations of a sounding or muted note. Muted attacks can send envelopes without creating a note; percussion attacks consume the mark without sending one. At release, the pitch wheel returns to the current accidental tuning, and each controller returns to its default or latest explicit `%%MIDI control` value at release in the same written voice. Overlay lanes use playback time for these defaults, so a setting later in playback never affects an earlier reset. Other notes sharing the channel can hear the effect. In cases where `abc2midi` shortens the gate, delays envelope samples until note-off, or moves later attacks, Symusic keeps the written rhythm and applies the curve over the sounding note.

`~` and `!roll!` play the same ornament. Use `%%MIDI harpmode 1` to perform either as three attacks at the host pitch instead of a cut from an adjacent note. The mode follows written order across voices and repeats; `%%MIDI harpmode 0` restores the cut. Dotted harp cuts follow abc2midi’s subdivision, which can differ from the written duration.

## Diagnostics

Declaring 2.1 or later with a file version line such as `%abc-2.1` or a header instruction such as `I:abc-version 2.1` enables stricter syntax checks. Use `!name!` for named decorations. To use `+name+`, set `I:decoration +` in the file or tune header. `I:linebreak !` also selects `+name+` and makes a bare `!` a layout break. Older and unversioned input accepts both decoration delimiters by default and tolerates a bare `!` where it cannot form a decoration.

Write a tie immediately after its first note or chord, as in `C- C`. With a 2.1 or later declaration, `C -C` warns and plays two attacks. Older and unversioned input accepts that detached tie.

Use `+:` to continue a header `H:` field. The reader skips unprefixed history lines and warns if the header declares 2.1 or later.

With a 2.1 or later declaration, a blank or whitespace-only line ends the tune. If it falls between `X:` and the required `K:`, the reader reports the missing `K:` there. Comment-only lines are skipped. The reader expects `T:` as the first field after `X:` and accepts an empty title. Misplaced fields warn and are ignored. For example, `M:` and `L:` can set file defaults, while `Q:` and `V:` belong inside a tune.

Python emits `UserWarning` for recoverable input, such as a stray closing bracket or a second tune. Each diagnostic gives the source, one-based line and UTF-8 byte column, and a code. Invalid UTF-8, embedded NUL, a missing tune header, an unclosed music group, division by zero or playback beyond the score's range raises `ValueError`. File access errors are reported separately.

Use `dumps_abc(warn=True)` or `dump_abc(path, warn=True)` for warnings about omissions, resampling and voice splitting. Invalid scores raise even with the default `warn=False`.

## Export timing and compatibility

Export begins with `%abc-2.1` and identifies the Symusic version in `I:abc-creator`. `I:linebreak <none>` leaves staff breaks to the engraver. The writer escapes literal text, including dollar signs. For line breaks it uses `\n` in voice names and a fixed-width Unicode escape in titles.

The writer validates event times in the score's own time unit and rounds them to the 480-tick grid. It writes note pitches and velocities, programs, tempos, keys, meters, supported control changes and explicit MIDI pitch bends. A score without an opening time signature gets `M:none`. Ties, tuplets and extra voices represent the performed notes. If an audible note starts at tick zero, all note onsets and aligned lyrics shift one tick. Audible notes that round down to zero duration get one tick. Pedal intervals that collapse are omitted.

For ordinary melodic notes the writer uses `%%MIDI expand 1/480` to recover the final tick of sound. For percussion and notes split into ties it uses fractional written lengths, including half-tick continuations. On reimport, Symusic recovers the release on the 480-tick grid, even when the same pitch starts again at that tick. The intervening controls, music fields and lyrics keep their positions.

The writer preserves the integer number of microseconds per quarter throughout the 24-bit MIDI tempo range. It uses a quarter-note beat in `Q:` when an integer BPM can reproduce the tempo, and shorter beats when necessary.

Timed `M:`, `K:`, and `Q:` changes are written in each voice that continues beyond the change. The first voice also carries changes after its own music so they survive export. A change can split a held note into tied segments. Symusic preserves the release; abc2midi can release it one tick early and can place repeated key or meter changes on different earlier MIDI events.

Lyrics are written as `w:` fields. Each written voice can carry one syllable at a given tick. The writer prefers new attacks, then uses tied continuations of held notes. It omits empty lyrics and lyrics without a free slot. abc2midi can move lyrics on tied continuations to the start of the tie and split syllables longer than its MIDI lyric buffer. Symusic preserves the timing and text of written syllables. Score markers are omitted.

The exporter splits dense chords across voices. It rejects plans that exceed the playback work limit, 15 melodic voices, or 30 total voices. Placement can retry within an onset, but does not reassign earlier onsets. If the search reaches its score-wide retry limit, the error identifies the onset and distinguishes exhaustion from placement failure. Use MIDI export if ABC cannot place the score.

In abc2midi, percussion voices share channel 10, so their releases and settings can interact. A one-tick drum hit can shrink to zero. Controllers can move earlier or follow a same-tick attack. Pitch bends can move earlier and shift later notes. Use MIDI export when bend timing must agree across players. The MIDI output can also lose voice names and control-only tracks.

Before an immediate melodic reattack on the same pitch, the writer can tie the preceding note into its last tick. This lets abc2midi release it one tick early and keep the new attack. Other chord members ending at the same boundary can also be shortened in abc2midi. A one-tick note has no room for the tie. The writer splits long rests into bounded lengths and can warn about holds and MIDI delta times that exceed the converter's limits.
