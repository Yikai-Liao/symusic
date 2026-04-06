# Accessible Documentation Guide — Deep Reference

A comprehensive guide to creating accessible scientific documentation, based on the [Scientific Python Accessible Documentation Guide](https://learn.scientific-python.org/documentation/accessible-documentation/).

## Contents

| Section | Lines | Description |
|---------|-------|-------------|
| Core Accessibility Principles | 20–64 | Foundational principles: write it out, multiple options, semantic structures |
| Documentation Structure Requirements | 65–131 | Heading hierarchy, page titles, TOC, and organizational elements |
| Text Accessibility Guidelines | 132–193 | Grammar, plain language, technical terms, and descriptive links |
| Image Accessibility | 194–335 | Color, contrast, alt text, descriptions, and source data links |
| Video Accessibility | 336–412 | Contextual intros, player controls, captions, and flash safety |
| Accessibility Checklist | 413–454 | Pre-publishing checklist for structure, text, images, video |
| Testing Tools | 455–484 | Automated testing, color contrast checkers, screen readers |
| Resources | 485–499 | Standards, scientific Python guides, and framework references |

---

## Core Accessibility Principles

### 1. Write It Out

Text is the most adaptable format across different technologies and user needs:

- Screen readers can process text
- Text can be resized, reformatted, and translated
- Search engines index text content
- Text works across all devices and connection speeds

**Implication:** Always provide text-based alternatives for visual content.

### 2. Provide Multiple Options

Offer both text and images to explain concepts:

```markdown
<!-- Good: Both text explanation and visual -->
## Data Flow Architecture

The pipeline processes data in three stages: ingestion, transformation,
and output. Raw data enters through the ingestion layer, passes through
validation and cleaning in the transformation layer, and exits as
structured output.

![Data flow diagram showing ingestion, transformation, and output stages](images/data-flow.png)

*Figure 1: Data flow through the three processing stages. Raw data (left)
enters ingestion, passes through transformation (center), and exits as
structured output (right).*
```

### 3. Use Semantic Structures

Use HTML/Markdown elements as intended:

| Element | Correct Use | Incorrect Use |
|---------|-------------|---------------|
| Headings (`#`, `##`) | Document structure, hierarchy | Making text bold/large |
| Lists (`-`, `1.`) | Related items, sequences | Adding visual spacing |
| Bold (`**`) | Emphasis, key terms | All important text |
| Links (`[](url)`) | Navigation to related content | Decorative elements |
| Tables | Tabular data with relationships | Layout/formatting |

## Documentation Structure Requirements

### Heading Hierarchy

Use headings in order without skipping levels:

```markdown
<!-- Correct: Sequential hierarchy -->
# Page Title (H1)
## Major Section (H2)
### Subsection (H3)
### Another Subsection (H3)
## Another Major Section (H2)

<!-- Incorrect: Skipped H2 -->
# Page Title (H1)
### Subsection (H3)  <!-- Wrong: jumped from H1 to H3 -->
```

**Rules:**
- Only one H1 per page (the page title)
- Never skip heading levels (H1 to H3)
- Headings describe content, not formatting

### Page Titles and Navigation

Ensure consistency between page titles and navigation:

```yaml
# mkdocs.yml - Good: Titles match navigation
nav:
  - Installation: getting-started/installation.md  # Page title: "Installation"
  - Quick Start: getting-started/quickstart.md     # Page title: "Quick Start"

# Bad: Mismatched titles
nav:
  - Install: getting-started/installation.md       # Page title: "Installation Guide"
```

### Table of Contents

Provide navigation aids:

```markdown
<!-- Sphinx: Built-in TOC -->
.. contents:: Table of Contents
   :depth: 2
   :local:

<!-- MkDocs Material: Automatic with configuration -->
# mkdocs.yml
theme:
  features:
    - navigation.tabs
    - navigation.sections
    - toc.integrate
```

### Organizational Elements

Apply proper structure:

- **Paragraphs:** Break long text into digestible chunks (3-5 sentences)
- **Lists:** Group related items (use bulleted for unordered, numbered for sequences)
- **Admonitions:** Highlight important information (notes, warnings, tips)
- **Code blocks:** Separate code from prose with proper syntax highlighting

## Text Accessibility Guidelines

### Grammar and Punctuation

Write complete sentences with proper punctuation:

```markdown
<!-- Good: Complete sentences -->
The function accepts a NumPy array as input. It returns the computed
mean along the specified axis. If no axis is provided, it computes
the global mean.

<!-- Poor: Fragmented, hard to parse -->
accepts NumPy array. returns mean. axis optional - global if not specified
```

### Plain Language

Avoid unnecessary jargon:

| Instead of | Use |
|------------|-----|
| utilize | use |
| implement | create, build |
| leverage | use |
| facilitate | help, enable |
| prior to | before |
| subsequent to | after |
| in order to | to |

### Technical Terms and Acronyms

Define terms on first use:

```markdown
<!-- Good: Define on first use -->
The Random Forest (RF) algorithm combines multiple decision trees.
Each tree in the RF ensemble votes on the final prediction.

<!-- Good: Define acronyms -->
This package uses the Application Programming Interface (API) provided
by the National Oceanic and Atmospheric Administration (NOAA).
Subsequent NOAA API calls are cached for performance.

<!-- Poor: Undefined acronym -->
Configure the API endpoint in your YAML file before calling the NOAA service.
```

### Descriptive Link Text

Write links that describe their destination:

```markdown
<!-- Good: Descriptive links -->
For installation instructions, see the [Installation Guide](installation.md).
Learn about configuration options in [Configuring Your Environment](config.md).

<!-- Poor: Generic links -->
For installation instructions, click [here](installation.md).
Learn about configuration options [in this document](config.md).
```

## Image Accessibility

### Design Requirements

#### Color Considerations

Images must remain comprehensible in grayscale:

```python
# Good: Use colorblind-friendly palettes
import matplotlib.pyplot as plt

# Option 1: Built-in colorblind palette
plt.style.use('tableau-colorblind10')

# Option 2: Manual colorblind-safe colors
colors = ['#0077BB', '#EE7733', '#009988', '#CC3311', '#33BBEE']

# Option 3: Use seaborn colorblind palette
import seaborn as sns
palette = sns.color_palette("colorblind")
```

#### Red-Green Color Blindness

Avoid red-green combinations that overlap:

```python
# Poor: Red and green overlap
plt.plot(x, y1, color='red', label='Series A')
plt.plot(x, y2, color='green', label='Series B')

# Good: Distinguishable colors + markers
plt.plot(x, y1, color='#0077BB', marker='o', label='Series A')
plt.plot(x, y2, color='#EE7733', marker='s', label='Series B')
```

#### Color Contrast

Maintain minimum 4.5:1 contrast ratio for text:

```python
# Check contrast programmatically
def luminance(r, g, b):
    """Calculate relative luminance."""
    def channel(c):
        c = c / 255
        return c / 12.92 if c <= 0.03928 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * channel(r) + 0.7152 * channel(g) + 0.0722 * channel(b)

def contrast_ratio(color1, color2):
    """Calculate contrast ratio between two RGB colors."""
    l1 = luminance(*color1)
    l2 = luminance(*color2)
    lighter = max(l1, l2)
    darker = min(l1, l2)
    return (lighter + 0.05) / (darker + 0.05)

# Example: Check white text on blue background
ratio = contrast_ratio((255, 255, 255), (0, 119, 187))
print(f"Contrast ratio: {ratio:.1f}:1")  # Should be >= 4.5
```

#### Sizing and Zoom

Ensure readability at various zoom levels:

- Annotations should match body text size (~12-14pt equivalent)
- Lines should be at least 1.5pt thick
- Markers should be distinguishable at 50% zoom
- Test images at 200% zoom for high-DPI displays

#### Animation Safety

Prevent seizure-inducing content:

- No flashing more than 3 times per second
- Avoid rapid color changes
- Provide pause controls for any animation
- Consider `prefers-reduced-motion` CSS media query

### Context Requirements

#### Alt Text

Provide alt text that captures the information conveyed:

```markdown
<!-- Good: Informative alt text -->
![Line plot showing temperature increasing from 15C to 25C over
12 months, with steepest rise June-August](images/temperature.png)

<!-- Poor: Describes appearance, not information -->
![A colorful line graph](images/temperature.png)

<!-- Poor: Filename as alt text -->
![temperature.png](images/temperature.png)
```

**Alt text guidelines:**
- Describe the information, not the appearance
- Keep under 125 characters when possible
- Use empty alt (`alt=""`) only for decorative images
- Don't start with "Image of..." or "Picture of..."

#### Image Descriptions

Provide detailed descriptions separate from captions:

```markdown
![Scatter plot of height vs weight](images/scatter.png)

*Figure 3: Scatter plot of participant height versus weight (n=150).*

<details>
<summary>Detailed image description</summary>

The scatter plot shows participant height (y-axis, 150-200 cm) versus
weight (x-axis, 50-100 kg). Points form a positive linear correlation
(r=0.72). Three outliers appear above 190 cm height. The regression
line indicates approximately 0.5 kg weight increase per 1 cm height.
Male participants (blue circles) cluster higher on both axes than
female participants (orange triangles).

</details>
```

#### Source Data Links

Link to reproducible sources:

```markdown
![Analysis results](images/results.png)

*Figure 4: Analysis results from experiment batch 2024-01.*

**Reproducibility:**
- [Source notebook](notebooks/analysis.ipynb)
- [Raw data (CSV)](data/experiment-batch-2024-01.csv)
- [Plotting script](scripts/generate_figure4.py)
```

## Video Accessibility

### Contextual Introduction

Introduce videos before they appear:

```markdown
<!-- Good: Context before video -->
## Training the Model

The following 3-minute video demonstrates the model training process,
including data loading, hyperparameter configuration, and monitoring
training progress. The same steps are documented in text below the video.

<video controls>
  <source src="videos/training.mp4" type="video/mp4">
</video>

### Text Version: Training Steps
1. Load the preprocessed dataset...
```

### Player Controls

Ensure proper video controls:

```html
<!-- Good: Controls, no autoplay -->
<video controls>
  <source src="video.mp4" type="video/mp4">
  Your browser does not support video playback.
</video>

<!-- Poor: Autoplay, no controls -->
<video autoplay loop>
  <source src="video.mp4" type="video/mp4">
</video>
```

**Requirements:**
- Always include `controls` attribute
- Never use `autoplay` without explicit user consent
- Provide pause/play, volume, and fullscreen controls
- Include progress bar for seeking

### Captions and Transcripts

Provide closed captions and transcripts:

```html
<video controls>
  <source src="tutorial.mp4" type="video/mp4">
  <track kind="captions" src="tutorial-captions.vtt" srclang="en" label="English">
</video>

<details>
<summary>Video Transcript</summary>

**[0:00]** Welcome to this tutorial on data preprocessing.

**[0:15]** First, we'll import the necessary libraries...

**[0:30]** [Screen shows code editor with import statements]

...
</details>
```

### Flash Safety

Avoid content that could trigger seizures:

- No flashing lights or colors faster than 3 per second
- Test videos with photosensitive epilepsy analysis tools
- Provide warnings for unavoidable flashing content
- Ensure information is available in non-video format

## Accessibility Checklist

### Before Publishing

Use this checklist for every documentation page:

#### Structure
- [ ] Page title is descriptive and matches navigation label
- [ ] Headings follow proper hierarchy (no skipping levels)
- [ ] Table of contents or jump links provided for long pages
- [ ] Logical reading order (makes sense without visual styling)

#### Text
- [ ] Complete sentences with proper grammar
- [ ] Technical terms defined on first use
- [ ] Acronyms expanded on first use
- [ ] Link text describes destination (no "click here")
- [ ] Plain language where possible

#### Images
- [ ] All images have meaningful alt text
- [ ] Complex images have detailed descriptions
- [ ] Images work in grayscale
- [ ] No red-green only distinctions
- [ ] Text contrast is at least 4.5:1
- [ ] Annotations are readable at standard zoom
- [ ] Source data/notebooks linked where applicable

#### Video
- [ ] Videos have play/pause controls
- [ ] No autoplay
- [ ] Closed captions included
- [ ] Transcript provided
- [ ] No flashing faster than 3 times per second
- [ ] Content duplicated in text elsewhere

#### General
- [ ] Site search functional
- [ ] All interactive elements keyboard accessible
- [ ] No information conveyed by color alone
- [ ] Consistent visual styling throughout

## Testing Tools

### Automated Testing

```bash
# Lighthouse (Chrome DevTools)
# Open DevTools > Lighthouse > Accessibility

# axe-core CLI
npm install -g @axe-core/cli
axe https://your-docs-site.readthedocs.io/

# pa11y
npm install -g pa11y
pa11y https://your-docs-site.readthedocs.io/
```

### Color Contrast Checkers

- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [Colorblind Web Page Filter](https://www.toptal.com/designers/colorfilter)
- [Coblis Color Blindness Simulator](https://www.color-blindness.com/coblis-color-blindness-simulator/)

### Screen Reader Testing

Test with actual screen readers:
- **NVDA** (Windows, free)
- **VoiceOver** (macOS, built-in)
- **Orca** (Linux, free)

## Resources

### Standards and Guidelines
- [WCAG 2.1 Guidelines](https://www.w3.org/WAI/WCAG21/quickref/)
- [WebAIM](https://webaim.org/)
- [A11y Project](https://www.a11yproject.com/)

### Scientific Python Specific
- [Scientific Python Accessible Documentation](https://learn.scientific-python.org/documentation/accessible-documentation/)
- [Matplotlib Accessibility](https://matplotlib.org/stable/users/explain/accessibility.html)
- [Accessible Color Palettes for Data Visualization](https://www.nceas.ucsb.edu/sites/default/files/2022-06/Colorblind%20Safe%20Color%20Schemes.pdf)

### Documentation Framework Guides
- [Sphinx Accessibility](https://www.sphinx-doc.org/en/master/usage/accessibility.html)
- [MkDocs Material Accessibility](https://squidfunk.github.io/mkdocs-material/setup/accessibility/)
