import 'dart:math';

Random _rnd = Random();

String word({int maxLength = 5}) {
  const chars = 'abcdefghijklmnopqrstuvwxy';
  return String.fromCharCodes(Iterable.generate(_rnd.nextInt(maxLength), (_) {
    return chars.codeUnitAt(_rnd.nextInt(chars.length));
  }));
}

String sentence({int maxWords = 5, int maxWordLength = 5}) {
  String sentence = "";
  for (var i = 0; i < maxWords; i++) {
    if (sentence.length > 0) sentence += " ";
    sentence += word(maxLength: maxWordLength);
  }
  return sentence.toUpperCase() + ".";
}
