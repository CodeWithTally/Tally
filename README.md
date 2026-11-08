<p align="center">
  <img src="./logo.png" alt="Tally logo" width="340">
</p>

<h1 align="center">Tally</h1>

<p align="center">
  <strong>Learn to code. Learn to think.</strong>
</p>

<p align="center">
  A beginner-friendly programming language built to help young learners understand code,
  practise logical thinking, and turn simple ideas into working programs.
</p>

---

## What is Tally?

**Tally** is a programming language designed for people taking their first steps into coding.

Its syntax reads more like everyday instructions than traditional programming languages. Instead of worrying about complicated punctuation, learners can focus on the ideas that matter: **variables, decisions, loops, functions, and problem-solving**.

Tally also includes a drawing system, so you can see the results of your code, not just read them.

It's made for experimenting, learning from mistakes, and gradually building confidence. Tally is a place to get comfortable with programming before moving on to bigger things.

## See it in action

Here is a complete Tally program that draws a square:

```text
set pen color to "purple"
set pen size to 4

repeat 4 times:
    move forward 100
    turn right 90

say "I drew a square!"
```

A few readable instructions are enough to introduce repetition, movement, and angles.

Or build a simple guessing game:

```text
set secret to random(1, 10)
set guess to 0

while guess is not secret:
    ask "Guess a number from 1 to 10:" and store as number in guess

    if guess < secret:
        say "Too low!"
    if guess > secret:
        say "Too high!"

say "You got it!"
```

## Why Tally?

- **Easy-to-read code** — commands such as `say`, `repeat`, and `ask` are written in plain language.
- **Learn by doing** — write a program, run it, change something, and see what happens.
- **Visual programming** — make drawings using a pen, coordinates, colors, and movement.
- **Friendly feedback** — error messages explain problems in terms beginners can understand.
- **Step-by-step debugging** — follow execution and inspect variables to understand how a program works.
- **Room to grow** — start with a greeting, then explore logic, lists, functions, and small projects.

## Getting started

1. Install Tally from the latest release.
2. Create a .tally file, or download an example from this repo!
3. Enter some Tally code.
4. Open the file to execute it.
5. That's it!

You don't need prior programming experience to begin.

### Your first program

```text
ask "What's your name?" and store in name
say "Hello, {name}!"

set score to 0
repeat 5 times:
    add 1 to score

say "Your score is {score}"
```

This example covers input, variables, text interpolation, repetition, and output.

> **Tip:** Tally uses indentation to group instructions. Put a colon (`:`) after `if`, `repeat`, `while`, `for each`, and `function` blocks, then indent their contents using spaces.

## Language basics

The following examples reflect the language's current core syntax.

| Concept | Tally example |
| --- | --- |
| Output | `say "Hello!"` |
| Variables | `set points to 10` |
| Update a number | `add 5 to points` |
| Input | `ask "Your name?" and store in name` |
| Numeric input | `ask "Age?" and store as number in age` |
| Conditions | `if points >= 10:` |
| Repetition | `repeat 5 times:` |
| Conditional loops | `while points < 20:` |
| List iteration | `for each item in items:` |
| Lists | `set colors to ["red", "blue"]` |
| List access | `say colors[0]` |
| Functions | `function double(number):` |
| Random integers | `random(1, 6)` |
| Comments | `# This is a comment` |

### Conditions

```text
set points to 15

if points >= 10:
    say "Level complete!"
otherwise:
    say "Keep trying!"
```

### Lists and loops

```text
set colors to ["red", "blue", "green"]

for each color in colors:
    say color

append "purple" to colors
say length(colors)
```

### Functions

```text
function square(number):
    return number * number

say square(7)
```

### Drawing

The drawing pen starts at the center of the canvas, pointing upward. You can move it, rotate it, lift it, and change its color or thickness.

```text
clear drawing
set pen color to "teal"
set pen size to 3

pen up
go to -100, -50
pen down

repeat 3 times:
    move forward 120
    turn right 120
```

Other drawing instructions include `move backward`, `turn left`, and `go to x, y`.

### Built-in helpers

Tally includes useful functions such as `random()`, `range()`, `length()`, `round()`, `floor()`, `ceil()`, `sqrt()`, `abs()`, `number()`, `text()`, `upper()`, `lower()`, and `contains()`.

## Learning with Tally

Tally is intended for self-study, coding clubs, introductory workshops, and classroom activities. A simple learning path might look like this:

| Stage | Learn | Build |
| --- | --- | --- |
| 1. First steps | `say`, variables, input | A personalized greeting |
| 2. Decisions | Comparisons and `if` | A simple quiz |
| 3. Repetition | `repeat` and `while` | A number guessing game |
| 4. Creativity | Pen movement and colors | Geometric drawings |
| 5. Organizing code | Lists and functions | A small interactive project |

Teachers and workshop organizers can adapt these exercises to suit their students' age and experience.

## Project goals

We're building Tally around a few straightforward ideas:

1. **Clarity over complexity.** The language should be understandable without a long setup or steep learning curve.
2. **Useful error messages.** Mistakes should teach, not discourage.
3. **Immediate feedback.** Learners should be able to see what their programs do.
4. **Experimentation.** Changing code and testing ideas should feel natural.
5. **Accessibility.** Learning the fundamentals of programming shouldn't require expensive tools.

Tally is an evolving educational project. Its language features and tooling may change as we learn from people using it.

## Contributing and feedback

Tally is developed by a small team, with assistance from AI tools, including ChatGPT by OpenAI. Feedback from students, teachers, and other developers is valuable.

You can help by:

- Reporting bugs with a minimal code example and the result you expected.
- Suggesting clearer error messages or simpler syntax.
- Sharing beginner exercises and example programs.
- Improving documentation and accessibility.
- Testing Tally and describing where you got stuck.

### Acknowledgments

- **Artificial Intelligence** has been partially used for writing code and debugging. 

Use the repository's **Issues** tab to report a problem or share an idea. Before submitting a pull request, check the repository's contribution and licensing information.

## Frequently asked questions

**Is Tally meant to replace Python?**  
No. Tally focuses on introducing core programming concepts in an approachable way. Skills learned here can transfer to other languages.

**Do I need to know how to code?**  
No. Tally is designed for beginners.

**What are `.tally` files?**  
They're plain-text files containing Tally programs, which you can open, edit, and run using a compatible Tally runner.

**Can I use Tally to teach programming?**  
That's one of its main goals. The examples above are suitable starting points for lessons and workshops.

**Does Tally support everything a general-purpose language does?**  
No. Its focus is on the fundamentals. Advanced features such as modules, filesystem access, and networking are not part of the core editor described here.

## License

The project's license will be specified in a `LICENSE` file once selected. Until then, public access to the source code does not by itself grant permission to redistribute or modify it.

---

<p align="center">
  <strong>Made for curious minds and first-time programmers.</strong><br>
  Start small. Try things. Keep learning.
</p>
