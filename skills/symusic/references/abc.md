# ABC notation

Symusic reads ABC as performed music, not as an engraving. Use `Score.from_abc(text)` for a string or `Score(path)` for a file. The reader takes the first tune, expands parts and repeats, and produces notes, tracks, and global metadata at 480 ticks per quarter. You can request quarter-note or second times with `ttype`.

ABC 2.1 takes precedence where it specifies notation or playback behavior. Symusic uses `abc2midi` as a reference for unspecified playback conventions, such as the one-tick initial note offset and grace-note timing, and for MIDI extensions, such as accompaniment from chord symbols. Backing instruments and instrument changes may appear on separate tracks. Layout does not affect playback. ABC 2.1 section 4.14 defines the notation for segno, coda, Da Capo and Fine. At those marks, playback continues in written order as it does in `abc2midi`.

In ABC 2.1 input, a closing `:|` without a matching `|:` repeats from the latest double bar or repeat ending, or from the beginning if neither exists. An explicit `|:` takes precedence over intervening double bars. Unversioned and older input do not use double bars to infer the opening.

On entering a reordered part or skipping an ending, notes use the unit length and key signature at their written position. A part label inside a bar also preserves accidentals carried from earlier notes in that bar. Measure rests and implicit tuplet ratios use the written meter. A `K:` change adds timed key metadata when playback visits that field. As in abc2midi, closing an explicit overlay that splits into multiple lanes clears the accidentals carried within it.

Broken rhythm and tuplet ratios apply to written notes across part labels. Each note keeps its adjusted length when a part jump or repeat plays only part of the group. Timed fields and dynamics between the two notes of a broken pair take effect at the adjusted note boundary. A grace group can borrow from a host in the next written part when playback continues across the label. Skipping the grace group leaves that duration with the host.

`%%MIDI snt key pitch` retunes one MIDI key. The pitch is absolute in semitones, so `%%MIDI snt 60 61.5` assigns a pitch halfway between C-sharp and D to MIDI key 60. As in abc2midi, the change uses the preceding emitted event's tick. After a leading rest, that tick can still be zero. The changes appear in `Score.note_tunings`. Use MIDI export when the score needs a tuning change at an exact tick during a rest or a held note. A compatible MIDI device is needed to hear the retuning. Symusic's SoundFont renderer does not apply it.

When reading a file, `I:abc-include defaults.abh` or `%%abc-include defaults.abh` imports that header at the point of the instruction. Includes can be nested, and every name is relative to the score's directory. Unavailable files and cycles warn and are skipped. `Score.from_abc(text)` does not open referenced files.

ABC text uses UTF-8 by default. A file header can declare `I:abc-charset us-ascii`. Use ABC text escapes for characters outside ASCII in such a file. Each included header can declare its own charset. Convert files encoded with an ISO-8859 charset to UTF-8 before import.

`R:hornpipe` enables swing and sets a 2:1 broken-rhythm ratio. A later `%%MIDI ratio` can change that ratio; a later `R:hornpipe` resets it to 2:1. These settings follow written order across voices and apply when the second note of a pair is read. Repeats and part order replay the written timing without carrying the setting backward. As in `abc2midi`, a hornpipe opening in 4/4 swings eighth notes and one opening in 2/4 swings sixteenth notes. A later `M:` changes the meter without choosing a new swing subdivision. Free meter uses the 4/4 subdivision without imposing a measure length.

In `%%MIDI beat a b c n`, `n` is the spacing between strong beats, in meter beats. A value of `0` uses the current meter's numerator. A written `M:` field restores the meter's default accent spacing without changing the three velocities or an explicit `%%MIDI beatstring`. Returning to an earlier meter on a repeat jump does not reset the spacing.

Backing patterns and the downbeat at a repeat bar follow the last performed `M:` field. A meter change preserves the beats that have already elapsed in the bar.

`Z` and `X` rest for one full measure by default. Use a positive integer to rest for more measures. `Z2` and `X2` leave the melody silent for two measures but preserve both measures of accompaniment and both bar boundaries for lyric and symbol alignment.

Clef suffixes `+8` and `-8` shift playback by an octave independently of `octave=`. Selecting a clef without a suffix clears the clef shift but leaves the `octave=` setting in effect. A `K:` field containing only clef or transposition options does not change the key signature or clear accidentals carried within the measure. With `I:propagate-accidentals octave`, the carried accidental applies to the octave after both shifts.

ABC 2.1 `transpose=` in `V:` or `K:` replaces the selected voice's playback transposition. A closing header `K:` after a `V:` declaration sets the opening key signature for the whole tune but applies its `transpose=`, `octave=`, and clef options to that selected voice. Without a header voice declaration, those options set the opening defaults. `%%MIDI transpose` is a separate abcMIDI extension and does not replace the standard transposition setting.

`%%MIDI transpose n` sets the MIDI shift in semitones, and `%%MIDI rtranspose n` adds to it. The shift follows abc2midi's track order. The conductor and each generated track replay their written commands and pass their final shift to the next track. A relative command can therefore apply more than once. Use `K:` or `V:` with `transpose=` for a voice-specific shift.

An inline `V:` selects a voice without breaking the rhythm of that voice. A numeric ID such as `01` identifies the same voice as `1`. Text IDs are distinguished by their first 20 characters. Pitch options on `V:` apply to the following note. When `V:` occurs between a grace group and its main note, only the main note uses the new pitch options.

A `Q:` tempo can combine up to four beat lengths. The older forms `Q:120` and `Q:C=120` count the unit note length in effect at that point. ABC 2.1 input warns about these deprecated forms but still plays them. To specify 120 quarter notes per minute regardless of `L:`, use `Q:1/4=120`.

In ABC 2.1 input, each run of `w:` fields applies to the notes since the preceding run in the same voice. A single `w:` field can be placed after several music lines. Adjacent fields supply alternative verses, and an empty field leaves its passage unsung. Tied notes each take a syllable, but rests, spacers and grace notes do not. Each written chord or ornament takes one syllable. When the part order replays a part, its written verses cycle by occurrence. A leading stanza number in `w: 1.~Three blind mice` leaves the first note for `Three`. Lyrics accept ABC text escapes such as `\'e`, `&eacute;`, and `\u00e9`. An escaped character stays part of its syllable. For example, `\u002a` displays an asterisk without skipping a note.

When a repeat jumps back into a music line, syllables already sung in the current part visit are omitted until playback enters another line. Syllables on newly reached notes, including a later ending, keep their written alignment.

Select these rules with `%abc-2.1` or `I:abc-version 2.1`. Unversioned input and versions before 2.1 retain the legacy music-line alignment used by `abc2midi`, including its lyric slots for grace and ornament attacks.

In ABC 2.1 titles, voice names and lyrics, font switches `$0` through `$4` do not become text or take a lyric slot. `$$` displays a literal dollar sign. Export doubles dollar signs so text such as `$1` is not interpreted as a font switch.

Use `s:` to place chord symbols, annotations, or `!decorations!` on earlier written notes in the current voice. A `*` skips one note, `|` advances to the next bar, and adjacent `s:` lines can stack symbols on the same notes. Grace notes, rests, and spacers do not take symbol slots. The symbols have the same playback effects as the inline forms.

`U:` definitions apply from their written position across voices. Repeats, alternate endings and part order play the shorthand expansion that each note was written with.

In ABC 2.1 input, `&&` as used in the standard's voice-overlay example plays the next two bar-delimited measures alongside the two just completed. Use ordinary bars inside the overlay; a repeat or section may close at its final bar. Lyric and symbol slots still follow the written order of the notes, not their playback time.

A trailing backslash continues the music across comments, fields, and directives. The continued music keeps one line for lyric alignment. If a tempo field occurs before a continued chord closes, the new tempo applies to the whole chord.

An inline `[r:...]` remark does not interrupt a note, chord, grace group, or macro. For example, `C[r:remark]2` plays as `C2`. A remark-like string inside quoted text is preserved.

In ABC 2.1 input, a tie must immediately follow its first note or chord. `C- C` sustains one note; `C -C` warns and plays two attacks. Comments, inline remarks, and reserved music characters do not count as intervening notation.

Older and unversioned input follows abc2midi's rule that `%%MIDI expand` skips tied notes. In ABC 2.1 input, expand also applies to ties. The writer uses a one-tick extension to preserve releases on a native round trip.

In older and unversioned input, notes on MIDI channel 10 keep their performed lengths under `%%MIDI trim`, `%%MIDI expand`, and stress model 1. Stress velocities, stress model 2 timing, and written staccato still apply.

With `%%MIDI expand`, spreading a chord can shift pending releases from preceding notes and ornament attacks, as in abc2midi. Symusic keeps subsequent onsets on the performed clock even when a pending release extends past them. Use `%%MIDI chordattack 0` to keep the calculated releases.

In ABC 2.1 input, the first note in a chord sets the time of the next event. Each member uses its own duration, and a suffix after the closing bracket multiplies every duration. In `[C2E3]2 D`, D starts after four units and E lasts six units before articulation. If a tied member overlaps its continuation, one attack holds through the later release.

In older and unversioned input, the first member also sets the length of the untied melodic notes in a chord, including its staccato shortening. Channel 10 keeps its individual member lengths. A suffix such as `]2` replaces each length with two units, overriding staccato shortening for that chord. A suffix written as `]1` or `]1/1` leaves the lengths as written.

Both members of `[DD]` are retained. The default MIDI spread delays each successive chord attack by 10 ticks, including a repeated pitch. Use `%%MIDI chordattack 0` for simultaneous attacks. The spread leaves the chord's written duration and the following note's onset intact.

Chord symbols accept equivalent qualities such as `Amin7` and `Am7`, or `A+7` and `Aaug7`. An exact `%%MIDI chordname` definition takes precedence over the built-in spelling. In `"C9/D"`, D is the lowest tone of the chord; in `"C9/A"`, A is a separate bass note below the chord. Roots and slash basses accept doubled ASCII accidentals as in `"Cbb"` and `"D/C##"`. Accidentals can cross the octave boundary without changing register: `Cb` sounds below C, and `B#` sounds above B. A parenthesized alternate, as in `"C9/D(Em)"`, does not change playback.

`%%MIDI chordname` registers a name for every voice from that point in the written tune. A custom name needs one to seven bytes and one to ten semitone intervals. Once a symbol is recognized, it uses the final registered voicing throughout playback, including on repeats. As in `abc2midi`, redefining the latest newly registered name once leaves its first definition in effect. Define each name once for predictable playback across players.

An unrecognized quality produces a warning. A voice with a recognized chord symbol plays unknown qualities as major chords on the written roots. A voice needs at least one recognized chord symbol to create chord backing.

Inside a chord, a dot shortens only the following member. A dot before the opening bracket shortens the whole chord. Inner dynamics take effect at the marked member and carry forward in written order. Fingering marks such as `!1!` do not affect playback. In ABC 2.1 input, `!tenuto!` uses the full written duration for the marked note, chord, or chord member, overriding `%%MIDI trim` and `%%MIDI expand` without moving the next attack. `!invertedfermata!` plays the same hold as `!fermata!`.

`%%MIDI randomchordattack n` replaces the fixed `%%MIDI chordattack` gap with a random gap of zero to `n-1` ticks. Set `n` to zero to restore the fixed gap. `!arpeggio!` triples the gap or range for the marked chord in ABC 2.1, or for the next chord in legacy input. In ABC 2.1, note ornaments attached to a `y` spacer or barline do not affect later notes. Tied continuations do not attack again, and members starting a tie inherit the current gap. Delays shorten the notes without shifting the next event. If a member is too short for its delay, Symusic limits the delays to keep written order and release times. Random gaps are repeatable but can differ from abc2midi's samples.

`%%MIDI ptstress 4 112 1.4 75 0.6 102 1.2 65 0.8` divides each measure into four equal written segments and gives each a MIDI velocity and a duration multiplier. The default model, `%%MIDI stressmodel 2`, stretches or compresses time within each measure, scaling all multipliers together so the full measure keeps its length. Model `1` instead sets the velocity and shortens gates without moving attacks; it leaves long notes outside its articulation window alone. The first profile supplies the tune default; subsequent body changes affect only later written notes. Put changes at a barline to keep the measure length unchanged. `%%MIDI beataccents` restores ordinary velocities and articulation without undoing model 2 timing. In a file, `%%MIDI ptstress "profile.txt"` reads the same numbers from a text file under the score's directory. String input does not open files. Invalid settings warn and keep the previous valid setting.

A fermata adds time before either stress model is applied. The added time shifts the stress position of later notes until the next written barline. Proportional grace notes borrow from the lengthened duration. If a playback change falls between a grace group and its host, the group borrows from the original duration to leave room for the host.

In older and unversioned input, fixed grace borrowing gives each member of the host chord the shortened length of its first member. With `%%MIDI grace`, the host members instead keep their written proportions, and all members of a grace chord contribute to the borrow weight. The first member still sets that grace chord's advance.

Staccato places the release halfway through the unwarped note. Model 1 calculates its articulation on that sounding interval. Model 2 maps the release and the full span separately, so adding a dot does not move later attacks. A dot inside a chord affects only its marked member.

Model 1 also samples stress separately for each attack in a trill or `R` roll. Model 2 stretches the host first, then uses its velocity for each generated attack without changing the velocities of later notes. `%%MIDI expand` and `%%MIDI trim 0/1` turn off stress shortening but keep stress velocities.

In model 1, a tied note that outlasts the stress window uses the ordinary beat velocity of its first attack. One-shot volume commands still apply at that attack, and a tied continuation never creates a new attack or shortens the gate just to apply stress.

ABC 2.1 input starts at `!mf!`; older and unversioned input retain abc2midi's default beat levels. `!pppp!` and `!ffff!` are equivalent to `!ppp!` and `!fff!`. In ABC 2.1 input, an accent increases the attacked note's velocity by one dynamic step, up to 127, without changing the current dynamic. An accent outside a chord applies to the chord; one inside applies only to its member. An explicit `%%MIDI vol` still sets the exact velocity.

As in abc2midi, the start and end of a crescendo or diminuendo each change the beat velocities by a fixed step. `%%MIDI deltaloudness n` changes the step from its default of 15 for marks written later, across voices. A repeat applies each mark's original step again without resetting the current velocity.

Within a grace group, `{a>b}` has the same relative lengths as `{a3/2b/2}`. This does not change the broken rhythm of the surrounding notes: `A<{a>b}A` still divides the two main notes in a 1:3 ratio.

Fixed grace-note lengths use the tuplet ratio active at the opening brace. For `(3C{d}EF`, the grace borrows time from `E` and counts separately from the three tuplet notes. Proportional grace uses the performed host length after tuplet scaling.

An ordinary `~` cut borrows time from its host for the upper neighbor. When the host spans three `L:` units after tuplets and fermata, `~` divides it into three attacks with upper and lower grace notes before the last two. For example, at `L:1/8`, `~D3` plays like `D {E}D {C}D`. An explicit grace group borrows from the first attack, and a tie to the next note starts at the last attack. `%%MIDI grace` controls the share of each host used by its grace notes. `%%MIDI expand` can extend grace releases into the host. `%%MIDI trim` applies to the main attacks.

A trill or `R` roll at the start of a tie spans the full tied duration. A trill or roll written on a continuation adds no attacks.

In ABC 2.1 input, named decorations use `!name!` unless the file or tune header selects `I:decoration +`. The legacy `I:linebreak !` makes a bare `!` layout-only and selects `+name+` decorations. Layout breaks do not advance playback or move lyric slots. Unversioned and older input still accepts bare `!` where it cannot form a valid decoration.

Use `m: ~G3 = G{A}G{F}G` for a static playback macro, or `m: ~n3 = n{o}n{m}n` to apply that pattern to any unaltered note written with length `3`. In the replacement, `n` is the written pitch, `o` is one scale step above it, and `m` is one below. Use spaces on both sides of the separator when the target contains a natural sign, as in `m: F=G = CD`. Macro definitions can be inherited from the file header or changed in written order within the tune. Macro expansion precedes `U:` shorthand replacement. Export writes the performed notes, not the original macro definitions.

Use `%%MIDIdef name command` to name a MIDI command and `%%MIDIx name …` to apply one or more previously defined commands inside a tune. Only percent-form definitions are recognized in the file header. The first valid definition of a name wins. An undefined name stops the expansion with a warning; commands before it still apply. A single `%%MIDIx` use combines its controller curves instead of replacing each one with the next. Use `%%MIDI programbase 1` inside a tune to number programs from 1 instead of 0. The last setting inside the tune applies to all voices and backing instruments.

Use `%%MIDI voice Bass instrument=59` to set the Bass voice to GM tuba, or omit `Bass` to set the current voice. The standard's `instrument=` and `bank=` are always one-based, regardless of `programbase`. A non-default bank is stored as Bank Select controls on the resulting track. Add `mute` to silence the voice's note attacks from that point, including overlays, without changing lyric alignment.

In ABC 2.1, `%%MIDI chordprog 20` selects Church Organ, General MIDI instrument 20. An explicit `%%MIDI programbase` overrides the numbering for `chordprog`. Older and unversioned files use the abc2midi default of zero-based programs.

Chord symbols and `droneon` apply to the melody or overlay lane where they appear. Their backing tracks follow that lane in the score. When a lane goes silent between overlays, its backing continues until switched off or the lane ends.

The drone reuses the MIDI delay of its initial Program Change for each of its two attacks, as in abc2midi. A drone that begins after a rest can start substantially later than the written command. End a drone with `%%MIDI droneoff` to emit its releases explicitly.

`%%MIDI control`, `portamento`, `noportamento`, and `pitchbend` accept an optional `bass` or `chord` target before their values. The target is a generated backing part of the current lane, not a named `V:` voice. Symusic writes the effect only to that part at the directive's performed time. abc2midi can also send these effects to the melody, time backing controllers earlier, or omit backing pitch bends.

Each melody or overlay lane can declare its own drum part with `%%MIDI drumon` or `[I:MIDI drumon]`. The part follows that lane's timing and appears after it in the score. `drumoff` switches off that lane without affecting the other parts. The older `!drum!` and `!nodrum!` marks also switch accompaniment on and off at the marked onset without restarting the pattern. They need a `drumon` declaration in the same lane, even if that declaration appears later or is in an unplayed part.

`%%MIDI drummap C 35` maps the written pitch `C` to percussion pitch 35. Before `X:` it sets a file default. Inside the tune it changes the map when the instruction is reached on its track. A separate conductor pass can leave a map change from the first voice in effect before its first audible note. Write the initial mapping inside the tune header to reapply it at the start of that voice. An overlay track starts with the map left by preceding tracks; changes in the overlay do not affect tracks already played.

In a key field such as `K:Dm exp _A`, `exp` leaves the D minor key signature in the score but replaces its implied accidentals with only those written in the field. Fractional accidentals in notes and key signatures produce MIDI pitch bends. Bends are shared by notes on a track: if overlapping notes require different bends, the reader warns and retains the channel's bend changes. Tuning applies in written order across voices and repeats. Use `%%temperament` for pitch-class offsets, or `%%MIDI temperamentlinear`, `%%MIDI temperamentequal`, and `%%MIDI temperamentnormal` to set the temperament. Inside the tune, `%%MIDI tuningsystem comma53` additionally quantizes the resulting pitches to a 53-step octave using abc2midi's mapping. A later temperament command does not turn this quantization off.

`%%MIDI makechordchannels n` assigns the next `n` ordinary chord attacks after the first to additional melodic routes. The first attack, notes outside a chord, and any excess chord attacks use the channel selected when the routes were allocated. A tied attack inherits the current route without consuming a slot; a tie continuation extends the original attack. Channel 10 applies the drum map rather than sounding transposition. The additional melodic routes remain subject to transposition even when the base route is percussion.

Every execution of `makechordchannels`, including on another repeat pass, allocates fresh channels with the current program. A zero or negative count leaves the allocation unchanged; counts above nine use nine. Channel numbers are allocated in MIDI track order across voices, overlay lanes, and backing tracks, not in order of note onset. `%%MIDI program channel program` addresses that numbered channel in the current lane. The reader reports an error if a chord allocation runs out of channels instead of reusing one already allocated.

Use `!bend!` with `%%MIDI bendstring`, `bendstringex`, or `bendvelocity` to bend the next attack. A mark before a grace group affects the first grace note; a mark after the group affects the main note. MIDI commands between the group and its host run after the grace attacks. Use `!shape!` to apply both a configured bend and the controller curves from `%%MIDI controlstring`. Each controlstring starts with a controller number followed by one or more values. It replaces the previous curves unless preceded by `%%MIDI controlcombo`; a shape can combine up to three controller curves.

An envelope spans the note's gate, including tied continuations of a sounding or muted note. Muted attacks can send envelopes without creating a note; percussion attacks consume the mark without sending one. At release, the pitch wheel returns to the current accidental tuning, and each controller returns to its default or latest explicit `%%MIDI control` value at release in the same written voice. Overlay lanes use playback time for these defaults, so a setting later in playback never affects an earlier reset. Other notes sharing the channel can hear the effect. In cases where `abc2midi` shortens the gate, delays envelope samples until note-off, or moves later attacks, Symusic keeps the written rhythm and applies the curve over the sounding note.

`~` and `!roll!` play the same ornament. Use `%%MIDI harpmode 1` to perform either as three attacks at the host pitch instead of a cut from an adjacent note. The mode follows written order across voices and repeats; `%%MIDI harpmode 0` restores the cut. Dotted harp cuts follow abc2midi’s subdivision, which can differ from the written duration.

## Diagnostics

Older files can continue a header `H:` field on lines without a field prefix. Symusic skips those history lines and warns in ABC 2.1 input. Use `+:` for new history continuations.

In ABC 2.1, a blank line, including a line with only spaces or tabs, ends the tune. If it follows `X:` but precedes the required `K:`, the reader reports the missing `K:` at the blank line. Comment-only lines do not end the tune.

ABC 2.1 fields outside their allowed positions warn and are ignored. For example, file headers can set `M:` and `L:` defaults, but `Q:` and `V:` belong inside a tune; a `K:` before `X:` does not close the tune header.

- **Recoverable input:** Python issues a `UserWarning` and returns the score, for example after ignoring a stray closing bracket or encountering a second tune. ABC 2.1 input also warns if the first field after `X:` is not `T:`. Comments are skipped and `T:` may be empty. Warnings include the source, one-based line, UTF-8 byte column, and a code. Fractional lengths such as `C/3` retain their written ratio until event times are quantized to ticks.
- **Invalid input:** Invalid UTF-8 or embedded NUL, a missing tune header, an unclosed music group, division by zero, or playback beyond the score's range raises `ValueError` with the same source format. Playback expansion and chord size are bounded. File access errors are reported separately.
- **Lossy export:** `dumps_abc(warn=True)` and `dump_abc(path, warn=True)` issue `UserWarning` for omissions, resampling, and voice splitting. With the default `warn=False`, these warnings are suppressed, but invalid scores still raise.

## Export timing and compatibility

Export begins with `%abc-2.1` and identifies the Symusic version in `I:abc-creator`. `I:linebreak <none>` leaves staff breaks to the engraver, independently of source wrapping. Titles and voice names escape literal quotes, ampersands, percent signs, backslashes, and control characters. Voice names use `\n` for line breaks. Titles use a fixed-width Unicode escape. In ABC 2.1 input, titles and voice names accept the standard mnemonics, named entities, and fixed-width Unicode escapes. Unversioned legacy names keep entity spellings literal.

ABC 2.1 free meter is stored as `TimeSignature(time, 0, 0)`. It has a quarter-note pulse and no downbeats. ABC export writes `M:none` for this event and when no opening time signature is supplied. MIDI export uses `4/4` for free meter because the MIDI meter event requires a numeric signature. Older and unversioned ABC input also exposes the numeric pulse.

Additive meters such as `M:(2+3+2)/8` accent the start of each group. The score preserves the written order in `TimeSignature.beat_groups` and includes one beat per group in its beat grid. ABC export writes the groups. MIDI export writes their total as the numerator.

Trailing rests that extend past the final event set `Score.explicit_end` in the score's time unit. It can extend `Score.end()`, and ABC export represents that silence as a rest in the first voice. The MIDI adapter ends tracks after their last MIDI event.

The writer emits playable ABC rather than reconstructing the original notation. It preserves note pitches, velocities, timing on its 480-tick grid, programs, tempos, keys, meters, supported control changes, and explicit MIDI pitch bends. It uses ties, tuplets, and additional voices when needed. If an audible note starts at tick zero, all note onsets and aligned lyrics shift one tick to accommodate the ABC playback offset. Export validates original event times in all three time units before rounding to the ABC grid. Short audible notes that collapse during rounding are extended to one tick; pedal intervals that collapse are omitted.

The writer preserves the integer number of microseconds per quarter throughout the 24-bit MIDI tempo range. It uses a quarter-note beat in `Q:` when an integer BPM can reproduce the tempo, and shorter beats when necessary.

`Score.copyright_notices` holds the text of conventional `Z:abc-copyright` fields and the deprecated `%%abc-copyright` directive. Each written notice is collected once, including notices inside repeated music. ABC export writes `Z:abc-copyright` in the tune header. MIDI export writes copyright metadata at tick zero. abc2midi recognizes the older directive and can timestamp or repeat its notices during playback.

Timed `M:`, `K:`, and `Q:` changes are written in each voice that continues beyond the change. The first voice also carries changes after its own music so they survive export. A change can split a held note into tied segments. Symusic preserves the release; abc2midi can release it one tick early and can place repeated key or meter changes on different earlier MIDI events.

Lyrics are written as `w:` fields. Each written voice can carry one syllable at a given tick. The writer prefers new attacks, then uses tied continuations of held notes. It omits empty lyrics and lyrics without a free slot. abc2midi can move lyrics on tied continuations to the start of the tie and split syllables longer than its MIDI lyric buffer. Symusic preserves the timing and text of written syllables. Score markers are omitted.

The exporter splits dense chords across voices. It rejects plans that exceed the playback work limit, 15 melodic voices, or 30 total voices. Placement can retry within an onset, but does not reassign earlier onsets. If the search reaches its score-wide retry limit, the error identifies the onset and distinguishes exhaustion from placement failure. Use MIDI export if ABC cannot place the score.

abc2midi reserves channel 10 for percussion. Multiple percussion voices share that channel, so their releases and channel settings can interact. abc2midi can shorten tied notes and drum hits, including reducing a one-tick drum hit to zero. It can move timed key, meter, and controller events to earlier MIDI events, or emit a controller after a note attack at the same tick; the export warnings identify these risks. Per-voice names and control-only tracks may not survive a MIDI round trip. Before an immediate same-pitch reattack, the writer ties a longer release so abc2midi emits Note Off one tick early instead of cutting off the new attack at the boundary. This also shortens other notes in the chord that end at that boundary; the exporter warns when requested. Symusic keeps the full duration. A one-tick reattack cannot use this workaround and warns when requested. abc2midi may also place a timed pitch bend early and shift subsequent notes. Use MIDI export when bend timing must agree across players. Long rests are split into bounded atoms to avoid length overflow in abc2midi. Very long holds and MIDI delta times can still exceed that converter’s limits; the exporter warns when it detects that risk. Native round trips and upstream playback are tested separately.
