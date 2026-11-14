# R21 Clock checker compile-time admission guard

R21 expresses the existing compile-time supervisor guard as
`if constexpr`, addressing MSVC C4127 under `/WX`. All five constants
remain false and the refusal still exits with code 10. R20's bounded
path copy and every target source remain in place.
